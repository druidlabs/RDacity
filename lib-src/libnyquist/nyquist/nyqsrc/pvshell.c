// pvshell.c -- This is a skeleton for a Nyquist primitive that
//   returns a sound. The sound is obtained by calling a function
//   with a request consisting of a location to put samples and
//   a count of how many samples are needed. The function returns
//   the actual number of samples computed and flags indicating
//   if the signal has reached the logical stop or termination.
//     In addition, there are interfaces for extracting samples
//   from input sounds. 
//     This code is designed for a time-stretching phase vocoder,
//   but could be used for other purposes. It is derived from
//   compose.c, which might have been implmented with this 
//   skeleton had we started out with this abstraction.

#include "stdio.h"
#ifndef mips
#include "stdlib.h"
#endif
#include "xlisp.h"
#include "sound.h"

#include "falloc.h"
#include "cext.h"
#include "pvshell.h"
#include "assert.h"

/* CHANGE LOG
 * --------------------------------------------------------------------
 * 28Apr03  dm  changes for portability and fix compiler warnings
 */

void pvshell_free();


typedef struct pvshell_susp_struct {
    snd_susp_node susp;
    long terminate_cnt;
    boolean logically_stopped;
    boolean started;

    pvshell_node pvshell;
} pvshell_susp_node, *pvshell_susp_type;


/* pvshell_test_f -- get next sample block and check flags
 *
 * Only call this from PVSHELL_TEST_F macro
 */
long pvshell_test_f(pvshell_type susp)
{
    long flags = 0;
    susp_get_samples(f, f_ptr, f_cnt); /* warning: macro references susp */
    if (susp->f->logical_stop_cnt == susp->f->current - susp->f_cnt) {
        flags |= PVSHELL_FLAG_LOGICAL_STOP;
    }
    if (susp->f_ptr == zero_block->samples) {
        flags |= PVSHELL_FLAG_TERMINATE;
    }
    return flags;
}


/* pvshell_test_g -- get next sample block and check flags
 *
 * Only call this from PVSHELL_TEST_G macro
 */
long pvshell_test_g(pvshell_type susp)
{
    long flags = 0;
    susp_get_samples(g, g_ptr, g_cnt); /* warning: macro references susp */
    if (susp->g->logical_stop_cnt == susp->g->current - susp->g_cnt) {
        flags |= PVSHELL_FLAG_LOGICAL_STOP;
    }
    if (susp->g_ptr == zero_block->samples) {
        flags |= PVSHELL_FLAG_TERMINATE;
    }
    return flags;
}


/* pvshell_fetch -- computes h(f, g, x, y) where f and g are 
 *  sounds, x and y are doubles, and h implemented via a function 
 *  pointer. This could certainly be generalized further, but 
 *  maybe we should take this one step at a time.
 */
void pvshell_fetch(snd_susp_type a_susp, snd_list_type snd_list)
{
    pvshell_susp_type susp = (pvshell_susp_type) a_susp;
    long n, flags;
    sample_block_type out;
    sample_block_values_type out_ptr;

    falloc_sample_block(out, "pvshell_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    /* don't run past the f input sample block: */
    /* most fetch routines call susp_check_term_log_samples() here
     * but we can't becasue susp_check_term_log_samples() assumes
     * that output time progresses at the same rate as input time.
     * Here, some time warping might be going on, so this doesn't work.
     * It is up to the user to tell us when it is the logical stop
     * time and the terminate time.
     */
    /* don't run past terminate time */
    //        if (susp->terminate_cnt != UNKNOWN &&
    //            susp->terminate_cnt <= susp->susp.current + cnt + togo) {
    //            togo = susp->terminate_cnt - (susp->susp.current + cnt);
    //            if (togo == 0) break;
    //        }
    /* don't run past logical stop time */
    //        if (!susp->logically_stopped && susp->susp.log_stop_cnt != UNKNOWN) {
    //            int to_stop = susp->susp.log_stop_cnt - (susp->susp.current + cnt);
    //            if (to_stop < togo && ((togo = to_stop) == 0)) break;
    //        }
    n = max_sample_block_len; // ideally, compute a whole block of samples

    flags = (susp->pvshell.h)(&(susp->pvshell), out_ptr, &n);

    /* test for termination */
    if (flags & PVSHELL_FLAG_TERMINATE) {
        snd_list_terminate(snd_list);
    } else {
        snd_list->block_len = n;
        susp->susp.current += n;
    }
    /* test for logical stop */
    if (flags & PVSHELL_FLAG_LOGICAL_STOP || susp->logically_stopped) {
        snd_list->logically_stopped = true;
        susp->logically_stopped = true;
    }
} /* pvshell_fetch */


void pvshell_mark(snd_susp_type a_susp)
{
    pvshell_susp_type susp = (pvshell_susp_type) a_susp;
    sound_xlmark(susp->pvshell.f);
    sound_xlmark(susp->pvshell.g);
}


void pvshell_free(snd_susp_type a_susp)
{
    pvshell_susp_type susp = (pvshell_susp_type) a_susp;
    /* note that f or g can be NULL */
    sound_unref(susp->pvshell.f);
    sound_unref(susp->pvshell.g);
    ffree_generic(susp, sizeof(pvshell_susp_node), "pvshell_free");
}


void pvshell_print_tree(snd_susp_type a_susp, int n)
{
    pvshell_susp_type susp = (pvshell_susp_type) a_susp;
    indent(n);
    stdputstr("f:");
    sound_print_tree_1(susp->pvshell.f, n);

    indent(n);
    stdputstr("g:");
    sound_print_tree_1(susp->pvshell.g, n);
}


sound_type snd_make_pvshell(char *name, rate_type sr, time_type t0,
                            h_fn_type h, sound_type f, sound_type g, 
                            double *state, long n)
{
    register pvshell_susp_type susp;
    int i;

    falloc_generic(susp, pvshell_susp_node, "snd_make_pvshell");
    susp->susp.fetch = pvshell_fetch;
    susp->terminate_cnt = UNKNOWN;

    /* initialize susp state */
    susp->susp.free = pvshell_free;
    susp->susp.sr = sr;
    susp->susp.t0 = t0;
    susp->susp.mark = pvshell_mark;
    susp->susp.print_tree = pvshell_print_tree;
    susp->susp.name = name;
    susp->logically_stopped = false;
    susp->susp.log_stop_cnt = UNKNOWN;
    susp->susp.current = 0;

    /* copy the sound so that we have a private "reader" object */
    susp->pvshell.f = (f ? sound_copy(f) : f);
    susp->pvshell.f_cnt = 0;

    susp->pvshell.g = (g ? sound_copy(g) : g);
    susp->pvshell.g_cnt = 0;
    
    susp->pvshell.h = h;

    susp->pvshell.flags = 0; /* terminated and logically stopped flags -- these
                                are for the client of pvshell to use */

    assert(n <= PVSHELL_STATE_MAX);
    for (i = 0; i < n; i++) {
        susp->pvshell.state[i] = state[i];
    }

    susp->started = false;
    return sound_create((snd_susp_type)susp, t0, sr, 1.0);
}
