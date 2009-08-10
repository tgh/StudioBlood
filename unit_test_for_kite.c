/*
 * Copyright (c) 2009 Tyler Hayes
 * ALL RIGHTS RESERVED
 *
 * Unit Test Driver
 *
 * This is a driver for unit testing the run() function of my sb_kite
 * LADSPA plugin.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "ladspa.h"
#include "xorgens.h"

// named switches (flags) for on/off (used in the unit tested)
#define ON 1
#define OFF 0

//---------------------------
//-- FUNCTION DECLARATIONS --
//---------------------------

// the unit to be tested
void run_Kite(LADSPA_Handle instance, unsigned long total_samples);

/*
 * These next functions are called by the unit being tested
 */
// gets a random unsigned long integer
unsigned long GetRandomNaturalNumber(unsigned long lower_bound,
                                     unsigned long upper_bound);

// reverses the given block of samples (block meaning an array of LADSPA_Data,
// which are floats)
void ApplyReverse(LADSPA_Data * buffer, unsigned long start, unsigned long end);

// copies a subsection of an array of LADSPA_Data (floats) into a subsection
// of another array
void CopySubBlock(LADSPA_Data * destination, unsigned long dest_start,
                  LADSPA_Data * source, unsigned long source_start,
                  unsigned long source_end, FILE * file);


//------------------------
//-- STRUCT DEFINITIONS --
//------------------------

typedef struct {
    // the samples per second of the sound
    unsigned long sample_rate;
    // data locations for the input & output audio ports
    LADSPA_Data * Input_Left;
    LADSPA_Data * Input_Right;
    LADSPA_Data * Output_Left;
    LADSPA_Data * Output_Right;
} Kite;


//----------------------
//-- GLOBAL VARIABLES --
//----------------------

// to hold the filename passed in by the user through the command line
char * filename;


//----------
//-- MAIN --
//----------
/*
 * takes three arguments, the sample rate, the number of samples, and
 * a file name to which the test data will be stored in.
 */
int main(int argc, char * argv[])
{
    // exit if run without 3 arguments
    if (argc != 4)
    {
        printf("\nNeed 3 arguments: sample rate, number of samples,");
        printf(" and filename");
        printf(" for test results (in that order).\n");
        exit(-1);
    }

    // get the file name
    filename = malloc(sizeof(char) * strlen(argv[3]));
    if (!filename)
        exit(-1);
    strcpy(filename, argv[3]);

    // set the appropriate constants from the command line arguments
    const LADSPA_Data SAMPLE_RATE = (const LADSPA_Data) atof(argv[1]);
    const unsigned long BUFFER_SIZE = (unsigned long) atol(argv[2]);

    // create a pseudo Reverse instance
    Kite * kite = malloc(sizeof(Kite));
    if (!kite)
    {
        free(filename);
        exit(-1);
    }

    kite->sample_rate = SAMPLE_RATE;

    /*
     * create psuedo input buffers of audio samples for the left and right
     * channels.
     * the sample values are arbitrary, but they are sequential in order
     * to read the output easier.
     */
    // setup the left channel input buffer
    kite->Input_Left = malloc(sizeof(LADSPA_Data) * BUFFER_SIZE);
    // exit if malloc failed
    if (!kite->Input_Left)
    {
        free(filename);
        free(kite);
        exit(-1);
    }
    // for loop index
    int i = 0;
    // arbitrary value to be put into the input buffers
    LADSPA_Data sample_val = 0.0f;
    // fill the left channel input buffer with sequential positive numbers
    for (i = 0; i < BUFFER_SIZE; ++i)
    {
        kite->Input_Left[i] = sample_val;
        sample_val += 1.0f;
    }
    
    // setup right channel input buffer
    kite->Input_Right = malloc(sizeof(LADSPA_Data) * BUFFER_SIZE);
    // exit if malloc failed
    if (!kite->Input_Right)
    {
        free(filename);
        free(kite->Input_Left);
        free(kite);
        exit(-1);
    }
    // fill the right channel input buffer with sequential negative numbers
    sample_val = 0.0f;
    for(i = 0; i < BUFFER_SIZE; ++i)
    {
        kite->Input_Right[i] = sample_val;
        sample_val -= 1.0f;
    }

    /*
     * create the output buffers and initialize them to all zeroes
     */
    // setup the left channel output buffer
    kite->Output_Left = malloc(sizeof(LADSPA_Data) * BUFFER_SIZE);
    // exit if malloc failed
    if (!kite->Output_Left)
    {
        free(filename);
        free(kite->Input_Left);
        free(kite->Input_Right);
        free(kite);
        exit(-1);
    }
    // fill the left channel output buffer to zeroes
    for (i = 0; i < BUFFER_SIZE; ++i)
        kite->Output_Left[i] = 0.0f;
    
    // setup the right channel output buffer
    kite->Output_Right = malloc(sizeof(LADSPA_Data) * BUFFER_SIZE);
    // exit if malloc failed
    if (!kite->Output_Right)
    {
        free(filename);
        free(kite->Input_Left);
        free(kite->Input_Right);
        free(kite->Output_Left);
        free(kite);
        exit(-1);
    }
    // fill the right channel output buffer to zeroes
    for (i = 0; i < BUFFER_SIZE; ++i)
      kite->Output_Right[i] = 0.0f;

    // add this test to the test log file
    FILE * fd = NULL;
    fd = fopen("test_log.txt", "a");
    fprintf(fd, "\nTest : %f sample rate, %ld samples\n", SAMPLE_RATE,
            BUFFER_SIZE);
    fprintf(fd, "\tResult:");
    fclose(fd);

    // run the unit to be tested
    run_Kite(kite, BUFFER_SIZE);

    // free dynamic memory and exit
    free(kite->Input_Left);
    free(kite->Input_Right);
    free(kite->Output_Left);
    free(kite->Output_Right);
    free(kite);
    free(filename);

    return 0;
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

    // set the minimum index of the random sub-blocks to 0.25 seconds
    const unsigned long MIN_BLOCK_START = (unsigned long)
                                          (0.25 * kite->sample_rate);
    // set the maximum index of the random sub-block to 2.25 seconds
    const unsigned long MAX_BLOCK_END = MIN_BLOCK_START +
                                        (2 * kite->sample_rate);
    // buffer pointers
    LADSPA_Data * input = NULL;
    LADSPA_Data * output = NULL;
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

//----------------FILE INITIATION FOR TEST RESULTS-----------------------------
    FILE * write_file = NULL;
    write_file = fopen(filename, "w");
    if (!write_file)
    {
      printf("\n**Error: fail to create file %s\n", filename);
      return;
    }
    fprintf(write_file, "Sample Rate: %ld", kite->sample_rate);
    fprintf(write_file, "\nSample Count: %ld\n", total_samples);
//-----------------------------------------------------------------------------
    
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
        
        // switch (or flag) for whether the sub-block should be reversed
        // (I wish C had boolean types)
        short reverse = OFF;
        // get a random state for reverse (on or off, which is 0 or 1)
        reverse = (short) GetRandomNaturalNumber(0, 1);

        // reverse the sub-block if the reverse switch is on
        if (reverse == ON)
        {
            // reverse the sub-block left channel
            ApplyReverse(kite->Input_Left, block_start_position,
                         block_end_position);
            // reverse the sub-block right channel
            ApplyReverse(kite->Input_Right, block_start_position,
                         block_end_position);
        }
        
//-----------------------------------------------------------------------------
        fprintf(write_file, "\nSub-block sample size: %ld", block_end_position -
                block_start_position + 1);
        fprintf(write_file, "\nReverse: %d\n\n", reverse);
        fprintf(write_file, "\nSample values for LEFT CHANNEL:\n\n");
//-----------------------------------------------------------------------------
        
        // append the sub-block to the output buffer for the left channel
        CopySubBlock(kite->Output_Left, out_index, kite->Input_Left,
                     block_start_position, block_end_position, write_file);

//-----------------------------------------------------------------------------
        fprintf(write_file, "\n\nSample values for RIGHT CHANNEL:\n\n");
//-----------------------------------------------------------------------------

        // append the sub-block to the output buffer for the right channel
        CopySubBlock(kite->Output_Right, out_index, kite->Input_Right,
                     block_start_position, block_end_position, write_file);
        
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
        
//-----------------------------------------------------------------------------
        fprintf(write_file, "\n\nOverwriting values for LEFT CHANNEL:\n\n");
//-----------------------------------------------------------------------------
        
        // write over used section of left channel input
        CopySubBlock(kite->Input_Left, block_start_position,
                     kite->Input_Left, source_start, samples_remaining - 1,
                     write_file);

//-----------------------------------------------------------------------------
        fprintf(write_file, "\n\nOverwriting values for RIGHT CHANNEL:\n\n");
//-----------------------------------------------------------------------------

        // write over used section of right channel input
        CopySubBlock(kite->Input_Right, block_start_position,
                     kite->Input_Right, source_start, samples_remaining - 1,
                     write_file);
        
        // update the output index
        out_index += samples_copied;
        // update the number of samples remaining to be processed
        samples_remaining -= samples_copied;
    }
    
//-----------------------------------------------------------------------------
    fclose(write_file);
//-----------------------------------------------------------------------------
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
    rand_num = xor4096i((unsigned long)(current_time.tv_usec *
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
                  unsigned long src_end, FILE * file)
{
    if (dest_start == src_start || src_start > src_end)
//-----------------------------------------------------------------------------
    {
        fprintf(file, "No need to overwrite.\n\n");
//-----------------------------------------------------------------------------
        return;
//-----------------------------------------------------------------------------
    }
//-----------------------------------------------------------------------------
    
    unsigned long dest_index = dest_start;
    unsigned long src_index = 0;
    
    for (src_index = src_start; src_index <= src_end; ++src_index)
    {
        destination[dest_index] = source[src_index];
        
//-----------------------------------------------------------------------------
        fprintf(file, "\n\t%f", source[src_index]);
//-----------------------------------------------------------------------------
        
        ++dest_index;
    }
}

// ------------------------------- EOF ----------------------------------------