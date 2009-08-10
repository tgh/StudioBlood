/*
 * Copyright © 2009 Tyler Hayes
 * ALL RIGHTS RESERVED
 * [This program is licensed under the GPL version 3 or later.]
 * Please see the file COPYING in the source
 * distribution of this software for license terms.
 *
 * This LADSPA plugin creates an overloaded fuzz distortion similar to The
 * Beatles' "Revolution".
 *
 * Thanks to:
 * - Bart Massey of Portland State University (http://web.cecs.pdx.edu/~bart/)
 *   for his direct help
 * - Richard Furse (http://www.muse.demon.co.uk/) for his examples
 * - David Benson (http://gdam.ffem.org/ladspa-doc/ladspa.html) for his
 *   tutorial
 * - Dave Phillips(http://linuxdevcenter.com/pub/a/linux/2001/02/02/ladspa.html)
 *   for his ladspa information
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
#define REVOLUTION_INPUT 0
#define REVOLUTION_OUTPUT 1

/*
 * Other constants
 */
// the plugin's unique ID given by Richard Furse (ladspa@muse.demon.co.uk)
#define UNIQUE_ID 4301
// number of ports involved
#define PORT_COUNT 2

//-------------------------
//-- FUNCTION PROTOTYPES --
//-------------------------

// calculates the average sample value of a given sample input stream
LADSPA_Data average_Sample_Value(LADSPA_Data * input,
                                 unsigned long sample_count);

//--------------------------------
//-- STRUCT FOR PORT CONNECTION --
//--------------------------------


typedef struct
{
    // data locations for the input & output audio ports
    LADSPA_Data * Input;
    LADSPA_Data * Output;
} Revolution;

//---------------
//-- FUNCTIONS --
//---------------


/*
 * Creates a Revolution plugin instance by allocating space for a plugin handle.
 * This function returns a LADSPA_Handle (which is a void * -- a pointer to
 * anything).
 */
LADSPA_Handle instantiate_Revolution()
{
    Revolution * revolution; // for a Revolution struct instance

    // allocate space for a Revolution struct instance
    revolution = (Revolution *) malloc(sizeof (Revolution));

    // send the LADSPA_Handle to the host.  If malloc failed, NULL is returned.
    return revolution;
}

//-----------------------------------------------------------------------------


/*
 * Make a connection between a specified port and it's corresponding data
 * location. For example, the output port should be "connected" to the place in
 * memory where that sound data to be played is located.
 */
void connect_port_to_Revolution(LADSPA_Handle instance, unsigned long Port,
                                LADSPA_Data * data_location)
{
    Revolution * revolution; // for a Revolution struct instance

    // cast the (void *) instance to (Revolution *) and set it to local pointer
    revolution = (Revolution *) instance;

    // direct the appropriate data pointer to the appropriate data location
    if (Port == REVOLUTION_INPUT)
        revolution->Input = data_location;
    else if (Port == REVOLUTION_OUTPUT)
        revolution->Output = data_location;
}

//-----------------------------------------------------------------------------


/*
 * Here is where the rubber hits the road.  The actual sound manipulation
 * is done in run().  For Revolution, it calculates the average of the samples
 * sent in from the host and cuts off any sample if it is above or below
 * +/- the average, thus "squaring off" the curved wave which produces
 * distortion. Once the sample is cut-off, it is increased to half-way between
 * the average and the maximum range (1.0 and -1.0).
 */
void run_Revolution(LADSPA_Handle instance, unsigned long sample_count)
{
    Revolution * revolution = (Revolution *) instance;

    /*
     * NOTE: these special cases should never happen, but you never know--like
     * if someone is developing a host program and it has some bugs in it, it
     * might pass some bad data.
     */
    if (sample_count <= 0)
    {
        printf("\nPlugin received zero or negative samples.");
        printf("\nPlugin not executed.\n");
        return;
    }
    if (!revolution)
    {
        printf("\nPlugin received NULL pointer for plugin instance.");
        printf("\nPlugin not executed.\n");
        return;
    }

    LADSPA_Data * input; // to point to the input stream
    LADSPA_Data * output; // to point to the output stream

    // link the local pointers to their appropriate streams passed in through
    // instance
    input = revolution->Input;
    output = revolution->Output;

    // calculate the average sample value
    LADSPA_Data avg_sample_val = average_Sample_Value(input, sample_count);

    // for each sample, cut the value off at +/- the average
    unsigned long i = 0;
    for (i = 0; i < sample_count; ++i)
    {
        // set the sample value to the average value if it is above the average
        // (or below the negative average), otherwise just copy the sample
        if (*input > avg_sample_val)
        {
            ++input;
            *(output++) = avg_sample_val;
        }
        else if (*input < -avg_sample_val)
        {
            ++input;
            *(output++) = -avg_sample_val;
        }
        else
            *(output++) = *(input++);
    }
}

//-----------------------------------------------------------------------------


/*
 * Frees dynamic memory associated with the Revolution instance.
 */
void cleanup_Revolution(LADSPA_Handle instance)
{
    if (instance)
        free(instance);
}

//-----------------------------------------------------------------------------

/*
 * Global LADSPA_Descriptor variable used in _init(), ladspa_descriptor(),
 * and _fini().
 */
LADSPA_Descriptor * revolution_descriptor = NULL;


/*
 * The _init() function is called whenever this plugin is first loaded
 * by the host using it.
 */
void _init()
{
    /*
     * allocate memory for revolution_descriptor (it's just a pointer at this
     * point). In other words create an actual LADSPA_Descriptor struct instance
     * that revolution_descriptor will point to.
     */
    revolution_descriptor = (LADSPA_Descriptor *)
            malloc(sizeof (LADSPA_Descriptor));

    // make sure malloc worked properly before initializing the struct fields
    if (revolution_descriptor)
    {
        // assign the unique ID of the plugin given by Richard Furse
        revolution_descriptor->UniqueID = UNIQUE_ID;

        /*
         * assign the label of the plugin. since there are no control features
         * for this plugin, "Revolution" is fine. (NOTE: it must not have white
         * spaces as per ladspa.h). NOTE: in case you were wondering, strdup()
         * from the string library makes a duplicate string of the argument and
         * returns the duplicate's pointer (a char *).
         */
        revolution_descriptor->Label = strdup("Revolution_Distortion");

        /*
         * assign the special property of the plugin, which is any of the three
         * defined in ladspa.h: LADSPA_PROPERTY_REALTIME,
         * LADSPA_PROPERTY_INPLACE_BROKEN, and LADSPA_PROPERTY_HARD_RT_CAPABLE.
         * They are just ints (1, 2, and 4, respectively).
         * See ladspa.h for what they actually mean.
         */
        revolution_descriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;

        // assign the plugin name
        revolution_descriptor->Name = strdup("Revolution");

        // assign the author of the plugin
        revolution_descriptor->Maker = strdup("Tyler Hayes (tgh@pdx.edu)");

        /*
         * assign the copyright info of the plugin (NOTE: use "None" for no
         * copyright as per ladspa.h)
         */
        revolution_descriptor->Copyright = strdup("GPL");

        /*
         * assign the number of ports for the plugin.  since there are no
         * control features for Revolution, there are only 2 ports: audio input
         * and output.
         */
        revolution_descriptor->PortCount = PORT_COUNT;

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
        revolution_descriptor->PortDescriptors = (const LADSPA_PortDescriptor *)
                temp_descriptor_array;

        /*
         * set the port properties by ORing specific bit masks defined in
         * ladspa.h.
         *
         * This first one gives the first port the properties that tell the host
         * that this port takes input and is an audio port (not a control port).
         */
        temp_descriptor_array[REVOLUTION_INPUT] = LADSPA_PORT_INPUT |
                LADSPA_PORT_AUDIO;

        /*
         * this gives the second port the properties that tell the host that
         * this port is an output port and that it is an audio port (I don't see
         * any situation where one might be an output port, but not an audio
         * port...).
         */
        temp_descriptor_array[REVOLUTION_OUTPUT] = LADSPA_PORT_OUTPUT |
                LADSPA_PORT_AUDIO;

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
        revolution_descriptor->PortNames = (const char **) temp_port_names;

        // set the name of the input port
        temp_port_names[REVOLUTION_INPUT] = strdup("Input");

        // set the name of the ouput port
        temp_port_names[REVOLUTION_OUTPUT] = strdup("Output");

        // reset temp variable to NULL for housekeeping
        temp_port_names = NULL;

        LADSPA_PortRangeHint * temp_hints; // temporary local variable (pointer
        // to a PortRangeHint struct) since revolution_descriptor->
        // PortRangeHints is a const *.

        // allocate space for two port hints (see ladspa.h for info on 'hints')
        temp_hints = (LADSPA_PortRangeHint *)
                calloc(PORT_COUNT, sizeof (LADSPA_PortRangeHint));

        /*
         * set the instance PortRangeHints pointer to the location temp_hints
         * is pointed at.
         */
        revolution_descriptor->PortRangeHints = (const LADSPA_PortRangeHint *)
                temp_hints;

        /*
         * set the port hint descriptors (which are ints). Since this is a
         * simple distortion effect without control, input and ouput don't need
         * any range hints.
         */
        temp_hints[REVOLUTION_INPUT].HintDescriptor = 0;
        temp_hints[REVOLUTION_OUTPUT].HintDescriptor = 0;

        // reset temp variable to NULL for housekeeping
        temp_hints = NULL;

        // set the instance's function pointers to appropriate functions
        revolution_descriptor->instantiate = instantiate_Revolution;
        revolution_descriptor->connect_port = connect_port_to_Revolution;
        revolution_descriptor->activate = NULL;
        revolution_descriptor->run = run_Revolution;
        revolution_descriptor->run_adding = NULL;
        revolution_descriptor->set_run_adding_gain = NULL;
        revolution_descriptor->deactivate = NULL;
        revolution_descriptor->cleanup = cleanup_Revolution;
    }
}

//-----------------------------------------------------------------------------


/*
 * Returns a descriptor of the requested plugin type (there is only one plugin
 * type in this library).
 *
 * NOTE: this function MUST be called 'ladspa_descriptor' or else the plugin
 * will not be recognized.
 */
const LADSPA_Descriptor * ladspa_descriptor(unsigned long index)
{
    if (index == 0)
        return revolution_descriptor;
    else
        return NULL;
}

//-----------------------------------------------------------------------------


/*
 * This is called automatically by the host when it is done with this plugin.
 * (When this dynamic library is unloaded).  It frees all dynamically
 * allocated memory associated with the descriptor.
 */
void _fini()
{
    if (revolution_descriptor)
    {
        free((char *) revolution_descriptor->Label);
        free((char *) revolution_descriptor->Name);
        free((char *) revolution_descriptor->Maker);
        free((char *) revolution_descriptor->Copyright);
        free((LADSPA_PortDescriptor *) revolution_descriptor->PortDescriptors);

        /*
         * the for loop here is kind of unnecessary since the number of ports
         * was hard coded for this plugin as 2, but whatever.
         */
        int i = 0;
        for (i = 0; i < revolution_descriptor->PortCount; ++i)
            free((char *) (revolution_descriptor->PortNames[i]));

        free((char **) revolution_descriptor->PortNames);
        free((LADSPA_PortRangeHint *) revolution_descriptor->PortRangeHints);

        free(revolution_descriptor);
    }
}

//-----------------------------------------------------------------------------


/*
 * This function is called from run_Revolution().  It calculates the average
 * value of all of the samples given from the host.
 */
LADSPA_Data average_Sample_Value(LADSPA_Data * input,
                                 unsigned long sample_count)
{

    // holds the running total of all sample values
    LADSPA_Data total = 0;
    // use a local pointer to cycle through the input stream
    LADSPA_Data * stream = input;
    // holds the number of samples actually used for the calculation
    unsigned long used_samples = sample_count;

    unsigned long i = 0;
    for (i = 0; i < sample_count; ++i)
    {
        /*
         * ignore any samples equal to zero, and reduce the amount of samples
         * to divide by later.  This will help the average from being too low.
         * For example, if you have a sound clip that has a lot of silent lead
         * time before the sound comes in and you want to just apply this effect
         * to the whole thing rather than highlighting the actual sound, the
         * average sample value might be too low for your liking.
         */
        if (*stream == 0.0f)
        {
            ++stream;
            --used_samples;
        }

            /*
             * you'll want the absolute value of the samples.  I could have used
             * abs() from math.h, but who cares.
             */
        else if (*stream < 0.0f)
            total += -(*(stream++));
        else
            total += *(stream++);
    }

    /*
     * NOTE: there is a problem here.  A long variable, such as sample_count,
     * is a 64-bit integer, and a LADSPA_Data variable (a float) is a 32-bit
     * variable.  If the value of the 64-bit variable is beyond the bounds of
     * the 32-bit type, then the 32-bit variable will get an unknown value when
     * the 64-bit variable is cast to the 32-bit type.  Will this situation ever
     * occur in this host/LADSPA plugin relationship?  I really don't know.
     * But it is dangerous to perform a narrowing cast such as this.
     */
    return (total / (LADSPA_Data) used_samples);
}

// EOF
