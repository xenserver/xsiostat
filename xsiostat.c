/*
 * -----------------------------
 *  XenServer Storage I/O Stats
 * -----------------------------
 *  xsiostat.c
 * ------------
 *
 * Copyright (c) 2013, 2014 Citrix Systems Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 only. with the special
 * exception on linking described in file LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */

// Header files
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/queue.h>
#include <sys/time.h>
#include "xsiostat.h"

// Helper functions
void
usage(char *argv0){
    // Local variables
    int                 i;              // Temporary integer

    // Print usage
    for (i=0; i<XSIS_PROGNAME_LEN+2; i++) fprintf(stderr, "-");
    fprintf(stderr, "\n %s\n", XSIS_PROGNAME);
    for (i=0; i<XSIS_PROGNAME_LEN+2; i++) fprintf(stderr, "-");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s [ -hs ] [ -i <interval> ] [ -o <out_file> ]" \
                    " [ -d <domain_id> [ ... ] ] [ -v <vbd_id> [ ... ] ]\n",
                    argv0);
    fprintf(stderr, "  -h            Print this help message and quit.\n");
    fprintf(stderr, "  -s            Scan for new VBDs at each iteration.\n");
    fprintf(stderr, "  -d            Filter for DOM ID (run list_domains for" \
                    " a list).\n");
    fprintf(stderr, "  -v            Filter for VBD ID (run xenstore-ls" \
                    " /local/domain/<dom_id>/device/vbd for a list).\n");
    fprintf(stderr, "  -i interval   Interval between outputs in" \
                    " milliseconds (1000 = 1s, default=%d).\n", XSIS_INTERVAL);
    fprintf(stderr, "  -o out_file   File to write the output to (in" \
                    " binary format).\n");
}

// Global variables
static uint32_t       unit = 1000000;   // MB/s
int                   PAGE_SIZE;

// Alarm handler
void
sigalarm_h(){
    return;
}

// Main loop
static void
main_loop(xsis_vbds_t *vbds){
    // Local variables
    static struct timeval now_0;        // Current time
    static struct timeval now_1;        // Time at last iteration
    float               now_diff;       // now_0 and now_1 time diff (secs)
    uint8_t             header = 0;     // Has the header been printed? (flag)
    xsis_vbd_t          *vbd;           // Temporary vbd iterator

    // Update time structures
    memcpy(&now_1, &now_0, sizeof(now_1));
    gettimeofday(&now_0, NULL);
    now_diff  = (float)(now_0.tv_sec-now_1.tv_sec);
    now_diff += ((float)now_0.tv_usec)/1000000;
    now_diff -= ((float)now_1.tv_usec)/1000000;

    // Loop through VBDs
    LIST_FOREACH(vbd, vbds, vbds){
        // Update VBD statistics
        if (vbd_update(vbd)){
            vbd_delete(vbd, vbds);
            continue;
        }

        // Print header
        if (!header){
            printf("----------------------------------------------------" \
                   "----------------------------------\n");
            printf("  DOM   VBD         r/s        w/s    rMB/s    wMB/s" \
                   " rAvgQs wAvgQs   Low_Mem_Mode\n");
            header = 1;
        }

        // Print general VBD info
        printf("%5d,%5d: ", vbd->domid, vbd->vbdid);

        // Print rw iops
        printf("%10.2f ",
               ((float)(vbd->tdstat.rop_0-vbd->tdstat.rop_1))/now_diff);
        printf("%10.2f ",
               ((float)(vbd->tdstat.wop_0-vbd->tdstat.wop_1))/now_diff);

        // Print rw throughput
        printf("%8.2f ",
               (((float)(vbd->tdstat.rsc_0-vbd->tdstat.rsc_1)*XSIS_SECTOR_SZ)/
                (float)unit)/now_diff);
        printf("%8.2f ",
               (((float)(vbd->tdstat.wsc_0-vbd->tdstat.wsc_1)*XSIS_SECTOR_SZ)/
                (float)unit)/now_diff);

        // Print average queue size
        printf("%6.2f ",
               ((float)(vbd->tdstat.rtu_0-vbd->tdstat.rtu_1))/
               (now_diff*1000000));
        printf("%6.2f",
               ((float)(vbd->tdstat.wtu_0-vbd->tdstat.wtu_1))/
               (now_diff*1000000));
        printf("%6d",vbd->tdstat.low_mem_mode);

        // Break line
        printf("\n");
    }

    // Flush if anything was printed
    if (header){
        printf("\n");
        fflush(stdout);
    }

    // Return
    return;
}

// Main
int
main(int argc, char **argv) {
    // Local variables
    xsis_flts_t         domids;         // List of DOM IDs to filter
    xsis_flts_t         vbdids;         // List of VBD IDs to filter
    xsis_vbds_t         vbds;           // List of attached VBDs
    int32_t             inter = -1;     // Report interval (ms)
    uint8_t             scan = 0;       // Scan for new VBDs (flag)
    uint8_t             reporting = 1;  // Currently reporting (flag)
    struct itimerval    itv;            // itimer setup
    char                *datafn = NULL; // Datafile pathname
    FILE                *datafp = NULL; // Datafile file pointer
    uint32_t            filter;         // Temporary filter
    int                 i;              // Temporary integer
    int                 err = 0;        // Return value

    // Initialise
    PAGE_SIZE = sysconf(_SC_PAGESIZE);
    LIST_INIT(&domids);
    LIST_INIT(&vbdids);
    LIST_INIT(&vbds);

    // Fetch arguments
    while ((i = getopt(argc, argv, "hsd:v:i:o:")) != -1){
        switch (i){
        case 's': // Set scan flag, if unset
            if (scan){
                fprintf(stderr, "%s: Invalid argument \"-s\", scan flag" \
                                " already set.\n", argv[0]);
                goto err;
            }
            scan++;
            break;

        case 'd': // Add DOM ID to filter
            filter = (uint32_t)strtoul(optarg, NULL, 10);
            if (flt_add(&domids, filter))
                goto err;
            break;

        case 'v': // Add VBD ID to filter
            filter = (uint32_t)strtoul(optarg, NULL, 10);
            if (flt_add(&vbdids, filter))
                goto err;
            break;

        case 'i': // Set output intervals, if unset
            if (inter != -1){
                fprintf(stderr, "%s: Invalid argument \"-i\", output" \
                                " interval already set.\n", argv[0]);
                goto err;
            }
            inter = (int32_t)strtoul(optarg, NULL, 10);
            break;

        case 'o': // Set datafile name
            if (datafn != NULL){
                fprintf(stderr, "%s: Invalid argument \"-o\", output"\
                                " datafile already set.\n", argv[0]);
                goto err;
            }
            if (!(datafn = strdup(optarg))){
                perror("strdup");
                fprintf(stderr, "%s: Error allocating memory for datafile" \
                                " name.\n", argv[0]);
                    goto err;
            }
            break;

        case 'h': // Print help
        default:
            usage(argv[0]);
            goto out;
        }
    }

    // Validate parameters and set defaults
    if (inter < 0)
        inter = XSIS_INTERVAL;

    if ((datafn != NULL) && (!(datafp = fopen(datafn, "w")))){
        perror("fopen");
        fprintf(stderr, "%s: Error opening datafile '%s' for writing.\n",
                argv[0], datafn);
        goto err;
    }

    // Allocate initial set of VBDs
    if (vbds_alloc(&vbds, &domids, &vbdids))
        goto err;

    // Set alarm
    signal(SIGALRM, sigalarm_h);
    itv.it_interval.tv_sec = inter/1000;
    itv.it_interval.tv_usec = (inter%1000)*1000;
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 100000;
    setitimer(ITIMER_REAL, &itv, NULL);

    // Loop
    while(!err){
        // Wait for alarm
        pause();

        // Update attached VBDs
        if (scan)
            err = vbds_alloc(&vbds, &domids, &vbdids);
        else
        if (LIST_EMPTY(&vbds)){
            // There are no VBDs to report and we are not scanning
            fprintf(stderr, "No VBDs to report and 'scan' flag not set.\n");
            break;
        }

        // Report
        if (!LIST_EMPTY(&vbds)){
            reporting = 1;
            main_loop(&vbds);
        } else if (reporting){
            printf("Waiting for VBDs to be plugged.\n");
            reporting = 0;
        }
    }

out:
    // Release resources
    vbds_free(&vbds);
    flts_free(&domids);
    flts_free(&vbdids);
    if (datafn)
        free(datafn);
    if (datafp)
        fclose(datafp);

    // Return
    return(err);

err:
    err = 1;
    goto out;
}
