/*
 * Copyright © 2009 Tyler Hayes
 * ALL RIGHTS RESERVED
 * [This program is licensed under the GPL version 3 or later.]
 * Please see the file COPYING in the source
 * distribution of this software for license terms.
 *
 * This is a LADSPA audio plugin that tries to emulate the ADT (Artifical
 * Double Tracking) technique invented by Ken Townsend, a studio technician
 * from Abbey Road Studios, for The Beatles.
 *
 * This plugin is ONLY FOR STEREO sound files.  Not only that, but it is
 * intended to be used with stereo files that have just been converted
 * from mono.  A mono sound in stereo is still a mono sound.  But using ADT
 * shifts the right channel of the mono sound just slightly (as in
 * milliseconds).
 * This does two things at once:  creates a panned stereo sound, and virtually
 * double-tracks the sound.  For example, you lay down a vocal track.  Then
 * apply ADT to it, and you automatically have two vocal tracks (sung literally
 * exactly the same), and panned into full stereo.  I say it's 'virtually'
 * double-tracked, because, of course, it is still only one sound file, and
 * thus, can only be one track.
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
#include "ladspa.h"


//-----------------------
//-- DEFINED CONSTANTS --
//-----------------------
/*
 * These are the port numbers for the plugin
 */
// left channel input
#define ADT_INPUT_LEFT 0
// right channel output
#define ADT_INPUT_RIGHT 1
// left channel input
#define ADT_OUTPUT_LEFT 2
// right channel output
#define ADT_OUTPUT_RIGHT 3
// control port for millisecond offset
#define ADT_OFFSET 4

/*
 * Other constants
 */
// the plugin's unique ID given by Richard Furse (ladspa@muse.demon.co.uk)
#define UNIQUE_ID 4305
// number of ports involved
#define PORT_COUNT 5
// the maximum and minimum milliseconds for the offset
#define MAX_OFFSET 200
#define MIN_OFFSET 5


//------------
//-- MACROS --
//------------
/*
 * This macro ensures the value used for the millisecond offset is between 5
 * and 200.
 */
#define LIMIT_BETWEEN_5_AND_200(x) (((x) < 5) ? 5 : (((x) > 200) ? 200 : (x)))


//-------------------------
//-- FUNCTION PROTOTYPES --
//-------------------------

// gets the offset value in samples
int GetOffsetInSamples(LADSPA_Data sample_rate, LADSPA_Data offset);


//--------------------------------
//-- STRUCT FOR PORT CONNECTION --
//--------------------------------


typedef struct
{
    // a buffer to hold the samples at the end of the right channel input
    // buffer that will be left out due to the offset of the output buffer
    LADSPA_Data * block_run_off;

    // the offset (in milliseconds) set by the user.
    // NOTE: the number has to be an integer between 5 and 200, but this
    // variable is a pointer to a LADSPA_Data (a float), because the connection
    // to data_location in the connect_port() function cannot be made unless
    // they are of the same type.
    LADSPA_Data * offset;

    // the sample rate of the audio
    LADSPA_Data sample_rate;

    // data locations for the input & output audio ports
    LADSPA_Data * Input_Left;
    LADSPA_Data * Input_Right;
    LADSPA_Data * Output_Left;
    LADSPA_Data * Output_Right;
} Adt;


//---------------
//-- FUNCTIONS --
//---------------


/*
 * Creates a plugin instance by allocating space for a plugin handle.
 * This function returns a LADSPA_Handle (which is a void * -- a pointer to
 * anything).
 */
LADSPA_Handle instantiate_Adt(const LADSPA_Descriptor * Descriptor,
                              unsigned long sample_rate)
{
    Adt * adt;
    // allocate space for a Adt struct instance
    adt = (Adt *) malloc(sizeof (Adt));
    // make sure malloc was a success
    if (adt)
    {
        // get the maximum offset in samples
        int sample_offset = GetOffsetInSamples((LADSPA_Data) sample_rate,
                                               (LADSPA_Data) MAX_OFFSET);
        // allocate space for the samples from the input buffer that will be
        // left out to the maximum amount of sample offset
        adt->block_run_off = malloc(sizeof (LADSPA_Data) * sample_offset);
        // return NULL if malloc failed
        if (!adt->block_run_off)
        {
            free(adt);
            return NULL;
        }
        // set the sample rate
        adt->sample_rate = (LADSPA_Data) sample_rate;
    }
    // send the LADSPA_Handle to the host.  If malloc failed, NULL is returned.
    return adt;
}

//-----------------------------------------------------------------------------


/*
 * This procedure just zero's out the block run off buffer inside the Adt
 * struct that's allocated in instantiate_Adt.  The other pointers inside
 * Adt are set to memory allocated by the host, which is why there are not
 * regarded here--only the ones allocated in instantiate_Adt, which is
 * block_run_off.
 */
void activate_Adt(LADSPA_Handle instance)
{
    Adt * adt = (Adt *) instance;
    // get the offset in samples in order to send the size of the buffer to
    // memset
    int sample_offset = GetOffsetInSamples(adt->sample_rate,
                                           (LADSPA_Data) MAX_OFFSET);
    /*
     * Richard Furse did this in his simple delay line plugin (part of the
     * ladspa SDK).  Here is his comment: "Need to reset the delay history
     * in this function rather than instantiate() in case deactivate()
     * followed by activate() have been called to reinitialise a delay line."
     * Of course, this isn't delay, but the idea is the same.  However, I
     * really don't understand it, seeing as neither that plugin nor this one
     * have a deactivate() function...
     */
    memset(adt->block_run_off, 0, sizeof (LADSPA_Data) * sample_offset);
}

//-----------------------------------------------------------------------------


/*
 * Make a connection between a specified port and it's corresponding data
 * location. For example, the output port should be "connected" to the place
 * in memory where that sound data to be played is located.
 */
void connect_port_to_Adt(LADSPA_Handle instance, unsigned long Port,
                         LADSPA_Data * data_location)
{
    Adt * adt;

    // cast the (void *) instance to (Adt *) and set it to local pointer
    adt = (Adt *) instance;

    // direct the appropriate data pointer to the appropriate data location
    switch (Port)
    {
        case ADT_INPUT_LEFT:
            adt->Input_Left = data_location;
            break;
        case ADT_INPUT_RIGHT:
            adt->Input_Right = data_location;
            break;
        case ADT_OUTPUT_LEFT:
            adt->Output_Left = data_location;
            break;
        case ADT_OUTPUT_RIGHT:
            adt->Output_Right = data_location;
            break;
        case ADT_OFFSET:
            adt->offset = data_location;
            break;
    }
}

//-----------------------------------------------------------------------------


/*
 * Here is where the rubber hits the road.  The actual sound manipulation
 * is done in run().
 * All it does is shift the right channel to the right the specified number
 * of samples (the offset).  It is intended to be used with mono audio files
 * after they have been converted to stereo to essentially "double-track" a
 * previously mono sound and those virtual tracks--if that makes sense...
 */
void run_Adt(LADSPA_Handle instance, unsigned long total_samples)
{
    Adt * adt = (Adt *) instance;

    /*
     * NOTE: these special cases should never happen, but you never know--like
     * if someone is developing a host program and it has some bugs in it, it
     * might pass some bad data.  If that's the case these printf's will help
     * the developer anyway!
     */
    if (total_samples <= 1)
    {
        printf("\nA sample count of 0 or 1 was sent to plugin.");
        printf("\nPlugin not executed.\n");
        return;
    }
    if (!adt)
    {
        printf("\nPlugin received NULL pointer for plugin instance.");
        printf("\nPlugin not executed.\n");
        return;
    }
    // for a one millisecond offest, a sample rate of 1000 is as low as you
    // would want to go.  Anything below that would risk having an offset of 0
    // samples, which is pointless.
    if (adt->sample_rate < 1000.0f)
    {
        printf("\nPlugin received a sample rate below 100 samples per second.");
        printf("\nPlugin not executed.\n");
        return;
    }

    // get the offset in samples
    const int SAMPLE_OFFSET = GetOffsetInSamples(adt->sample_rate,
                                                 *(adt->offset));

    // buffer pointers
    LADSPA_Data * input = NULL;
    LADSPA_Data * output = NULL;
    LADSPA_Data * run_off = adt->block_run_off;

    // buffer indexes
    unsigned long in_index = 0;
    unsigned long out_index = 0;
    int run_off_index = 0;

    // copy all left channel input buffer into left channel output buffer
    input = adt->Input_Left;
    output = adt->Output_Left;
    for (in_index = 0; in_index < total_samples; ++in_index)
    {
        output[out_index] = input[in_index];
        ++out_index;
    }
    // copy all samples from the run off buffer into the right channel output
    // buffer
    output = adt->Output_Right;
    out_index = 0;
    for (run_off_index = 0; run_off_index < SAMPLE_OFFSET; ++run_off_index)
    {
        output[out_index] = run_off[run_off_index];
        ++out_index;
    }
    // copy right channel input buffer into the rest of right channel output
    // buffer
    input = adt->Input_Right;
    in_index = 0;
    /*
     * NOTE: the 'out_index = out_index' statement is to keep the compiler from
     * giving a warning when using just 'out_index', because you don't want to
     * set out_index to anything but what it already is at this point.
     */
    for (out_index = out_index; out_index < total_samples; ++out_index)
    {
        output[out_index] = input[in_index];
        ++in_index;
    }
    // copy the left over samples from the right channel input buffer into the
    // run off buffer
    run_off_index = 0;
    /*
     * NOTE: the 'in_index = in_index' statement is to keep the compiler from
     * giving a warning when using just 'in_index', because you don't want to
     * set in_index to anything but what it already is at this point.
     */
    for (in_index = in_index; in_index < total_samples; ++in_index)
    {
        run_off[run_off_index] = input[in_index];
        ++run_off_index;
    }
}

//-----------------------------------------------------------------------------


/*
 * Frees dynamic memory associated with the plugin instance.
 */
void cleanup_Adt(LADSPA_Handle instance)
{
    Adt * adt = (Adt *) instance;

    if (!adt)
        return;

    if (adt->block_run_off)
        free(adt->block_run_off);
    free(adt);
}

//-----------------------------------------------------------------------------

/*
 * Global LADSPA_Descriptor variable used in _init(), ladspa_descriptor(),
 * and _fini().
 */
LADSPA_Descriptor * Adt_descriptor = NULL;


/*
 * The _init() procedure is called whenever this plugin is first loaded
 * by the host using it (when the host program is first opened).
 */
void _init()
{
    /*
     * allocate memory for Adt_descriptor (it's just a pointer at this point).
     * in other words create an actual LADSPA_Descriptor struct instance that
     * Adt_descriptor will point to.
     */
    Adt_descriptor = (LADSPA_Descriptor *) malloc(sizeof (LADSPA_Descriptor));

    // make sure malloc worked properly before initializing the struct fields
    if (Adt_descriptor)
    {
        // assign the unique ID of the plugin given by Richard Furse
        Adt_descriptor->UniqueID = UNIQUE_ID;

        /*
         * assign the label of the plugin.
         * NOTE: it must not have white spaces as per ladspa.h.
         * NOTE: in case you were wondering, strdup() from the string library
         * makes a duplicate string of the argument and returns the duplicate's
         * pointer (a char *).
         */
        Adt_descriptor->Label = strdup("ADT");

        /*
         * assign the special property of the plugin, which is any of the three
         * defined in ladspa.h: LADSPA_PROPERTY_REALTIME,
         * LADSPA_PROPERTY_INPLACE_BROKEN, and LADSPA_PROPERTY_HARD_RT_CAPABLE.
         * They are just ints (1, 2, and 4, respectively).  See ladspa.h for
         * what they actually mean.
         */
        Adt_descriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;

        // assign the plugin name
        Adt_descriptor->Name = strdup("ADT (Artificial Double Tracking)");

        // assign the author of the plugin
        Adt_descriptor->Maker = strdup("Tyler Hayes (tgh@pdx.edu)");

        /*
         * assign the copyright info of the plugin (NOTE: use "None" for no
         * copyright as per ladspa.h)
         */
        Adt_descriptor->Copyright = strdup("GPL");

        /*
         * assign the number of ports for the plugin.
         */
        Adt_descriptor->PortCount = PORT_COUNT;

        /*
         * used for allocating and initailizing a LADSPA_PortDescriptor array
         * (which is an array of ints) since Adt_descriptor-> PortDescriptors
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
        Adt_descriptor->PortDescriptors = (const LADSPA_PortDescriptor *)
                temp_descriptor_array;

        /*
         * set the port properties by ORing specific bit masks defined in
         * ladspa.h.
         *
         * these first two give the input ports the properties that tell the
         * host that the ports takes input and are audio ports (not control
         * ports).
         */
        temp_descriptor_array[ADT_INPUT_LEFT] = LADSPA_PORT_INPUT |
                LADSPA_PORT_AUDIO;
        temp_descriptor_array[ADT_INPUT_RIGHT] = LADSPA_PORT_INPUT |
                LADSPA_PORT_AUDIO;

        /*
         * this gives the output ports the properties that tell the host that
         * these ports are output ports and that they are audio ports (I don't
         * see any situation where one might be an output port but not an audio
         * port...).
         */
        temp_descriptor_array[ADT_OUTPUT_LEFT] = LADSPA_PORT_OUTPUT |
                LADSPA_PORT_AUDIO;
        temp_descriptor_array[ADT_OUTPUT_RIGHT] = LADSPA_PORT_OUTPUT |
                LADSPA_PORT_AUDIO;

        /*
         * this one gives the control port that defines the offset in
         * milliseconds the properties that tell the host that this port takes
         * input (from the user) and is a control port (a port that is
         * controlled by the user).
         */
        temp_descriptor_array[ADT_OFFSET] = LADSPA_PORT_INPUT |
                LADSPA_PORT_CONTROL;

        /*
         * set temp_descriptor_array to NULL for housekeeping--we don't need
         * that local variable anymore.
         */
        temp_descriptor_array = NULL;

        /*
         * temporary local variable (which is a pointer to an array of arrays
         * of characters) for the names of the ports since Adt_descriptor->
         * PortNames is a const char * const *.
         */
        char ** temp_port_names;

        // allocate the space for two port names
        temp_port_names = (char **) calloc(PORT_COUNT, sizeof (char *));

        /*
         * set the instance PortNames array pointer to the location
         * temp_port_names is pointing at.
         */
        Adt_descriptor->PortNames = (const char **) temp_port_names;

        // set the name of the input ports
        temp_port_names[ADT_INPUT_LEFT] = strdup("Input Left Channel");
        temp_port_names[ADT_INPUT_RIGHT] = strdup("Input Right Channel");

        // set the name of the ouput ports
        temp_port_names[ADT_OUTPUT_LEFT] = strdup("Output Left Channel");
        temp_port_names[ADT_OUTPUT_RIGHT] = strdup("Output Right Channel");

        // set the name of the control port for the millisecond offset
        temp_port_names[ADT_OFFSET] =
                strdup("Right channel offset (in milliseconds)");

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

        /*
         * set the instance PortRangeHints pointer to the location temp_hints
         * is pointed at.
         */
        Adt_descriptor->PortRangeHints = (const LADSPA_PortRangeHint *)
                temp_hints;

        /*
         * set the port hint descriptors (which are ints).
         */
        temp_hints[ADT_INPUT_LEFT].HintDescriptor = 0;
        temp_hints[ADT_INPUT_RIGHT].HintDescriptor = 0;
        temp_hints[ADT_OUTPUT_LEFT].HintDescriptor = 0;
        temp_hints[ADT_OUTPUT_RIGHT].HintDescriptor = 0;
        /*
         * For the control port, the BOUNDED masks from ladspa.h tell the host
         * that this control has limits (this one is 5 and 200 as defined in
         * the Macro at the top). The DEFAULT_LOW mask tells the host to set
         * the control value upon start (like for a gui) to a low value between
         * the bounds.
         * The INTEGER mask tells the host that the control values should be in
         * integers.
         */
        temp_hints[ADT_OFFSET].HintDescriptor = (LADSPA_HINT_BOUNDED_BELOW
                                                 | LADSPA_HINT_BOUNDED_ABOVE
                                                 | LADSPA_HINT_DEFAULT_LOW
                                                 | LADSPA_HINT_INTEGER);
        // set the lower bound of the control
        temp_hints[ADT_OFFSET].UpperBound = (LADSPA_Data) MAX_OFFSET;
        // set the upper bound of the control
        temp_hints[ADT_OFFSET].LowerBound = (LADSPA_Data) MIN_OFFSET;

        // reset temp variable to NULL for housekeeping
        temp_hints = NULL;

        // set the instance's function pointers to appropriate functions
        Adt_descriptor->instantiate = instantiate_Adt;
        Adt_descriptor->connect_port = connect_port_to_Adt;
        Adt_descriptor->activate = activate_Adt;
        Adt_descriptor->run = run_Adt;
        Adt_descriptor->run_adding = NULL;
        Adt_descriptor->set_run_adding_gain = NULL;
        Adt_descriptor->deactivate = NULL;
        Adt_descriptor->cleanup = cleanup_Adt;
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
        return Adt_descriptor;
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
    if (Adt_descriptor)
    {
        free((char *) Adt_descriptor->Label);
        free((char *) Adt_descriptor->Name);
        free((char *) Adt_descriptor->Maker);
        free((char *) Adt_descriptor->Copyright);
        free((LADSPA_PortDescriptor *) Adt_descriptor->PortDescriptors);

        /*
         * the for loop here is kind of unnecessary since the number of ports
         * was hard coded for this plugin as 2, but whatever.
         */
        int i = 0;
        for (i = 0; i < Adt_descriptor->PortCount; ++i)
            free((char *) (Adt_descriptor->PortNames[i]));

        free((char **) Adt_descriptor->PortNames);
        free((LADSPA_PortRangeHint *) Adt_descriptor->PortRangeHints);

        free(Adt_descriptor);
    }
}

//-----------------------------------------------------------------------------


/*
 * This function just converts the #defined offset from milliseconds to samples.
 */
int GetOffsetInSamples(LADSPA_Data sample_rate, LADSPA_Data offset)
{
    // convert the offset in milliseconds to seconds
    LADSPA_Data offset_seconds =
            (((LADSPA_Data) LIMIT_BETWEEN_5_AND_200((int) offset)) / 1000.0f);
    // convert the seconds to samples
    int offset_samples = (int) (sample_rate * offset_seconds);

    return offset_samples;
}

// ------------------------------- EOF ----------------------------------------
