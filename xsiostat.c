/*
 * -----------------------------
 *  XenServer Storage I/O Stats
 * -----------------------------
 *  xsiostat.c
 * ------------
 *
 * Copyright (C) 2013 Citrix Systems Inc.
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
 *
 * TODO:
 *   Include blktap pools metric.
 *   Consider requests merged. <- this could affects some of the stats
 *   Finish the data output code <- writing to datafile is currently ignored
 *   Write the interpretation code <- there's no way to read from a datafile
 */

// Header files
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include "xsiostat.h"

// Helper functions
void usage(char *argv0){
	// Local variables
	int		i;		// Temporary integer

	// Print usage
	for (i=0; i<MT_PROGNAME_LEN+2; i++) fprintf(stderr, "-");
	fprintf(stderr, "\n %s\n", MT_PROGNAME);
	for (i=0; i<MT_PROGNAME_LEN+2; i++) fprintf(stderr, "-");

#ifdef	MT_USE_TAPDISK
	fprintf(stderr, "\nUsage: %s [ -hx ] [ -d <domain_id> [ -v <vbd_id> ] ] [ -i <interval> ] [ -o <out_file> ]\n", argv0);
	fprintf(stderr, "       -h               Print this help message and quit.\n");
	fprintf(stderr, "       -x               Print extended statistics.\n");
#else
	fprintf(stderr, "\nUsage: %s [ -h ] [ -d <domain_id> [ -v <vbd_id> ] ] [ -i <interval> ] [ -o <out_file> ]\n", argv0);
	fprintf(stderr, "       -h               Print this help message and quit.\n");
#endif	/* MT_USE_TAPDISK */

	fprintf(stderr, "       -d               Domain ID (run list_domains for a list).\n");
	fprintf(stderr, "       -v               VBD ID (run xenstore-ls /local/domain/<dom_id>/device/vbd for a list).\n");
	fprintf(stderr, "       -i interval      Interval between outputs in milliseconds (1000 = 1s, default=%d).\n", MT_INTERVAL);
	fprintf(stderr, "       -o out_file      File to write the output to (in binary format).\n");
}

// Global variables
static mt_mempool_t	*mpools = NULL;		// List of blkback mempool entries, with associated vbds
static int		quit = 0;		// Global exit condition
#ifdef	USE_MT_TAPDISK
static int		extended = 0;		// Print extended statistics
#endif	/* USE_MT_TAPDISK */
static uint32_t		unit = 1000000;		// MB/s
struct timeval		now_0;			// Current time
struct timeval		now_1;			// Time at last iteration

// Alarm handler
void sigalarm_h(){
	return;
}

// Main loop
void main_loop(){
	// Local variables
	mt_mempool_t	*mpool;			// Temporary mpool iterator
	mt_vbd_t	*vbd;			// Temporary vbd iterator
	float		now_diff;		// Time difference (secs) between now_0 and now_1

	// Update time structures
	memcpy(&now_1, &now_0, sizeof(now_1));
	gettimeofday(&now_0, NULL);
	now_diff =  (float)(now_0.tv_sec-now_1.tv_sec);
	now_diff += ((float)now_0.tv_usec)/1000000;
	now_diff -= ((float)now_1.tv_usec)/1000000;

#ifdef	MT_USE_TAPDISK
	if (extended){
		fprintf(stdout, "------------------------------------------------------------------------------------------------------------------\n");
	}else{
		fprintf(stdout, "-------------------------------------------------------------------------------\n");
	}
#else
	fprintf(stdout, "----------------------------------------------------------------------\n");
#endif	/* MT_USE_TAPDISK */

	// Loop through all memory pools
	mpool = mpools;
	while (mpool) {
		// Update memory pool data
		if (mpool_update(mpool, MT_SYSFS_BBPOOL_FREE) != 0){
			quit = 1;
			return;
		}

		// Print memory pool usage
		fprintf(stdout, "pool: %s (%4d, %4d)\n", mpool->name, mpool->size, mpool->size - mpool->free);

		// Print header
#ifdef	MT_USE_TAPDISK
		if (extended){
		//                vbd: 123,12345: (123,123) (123,123) (1234.56,1234.56) (123.45,123.45) (123,123) (1234.56,1234.56) (123.45,123.45)
		fprintf(stdout, "      DOM   VBD  BLKBKRING  INFLIGHT     BLKTAP REQS/s     BLKTAP MB/s   TD INFL    TAPDISK REQS/s    TAPDISK MB/s\n");
		fprintf(stdout, "                  TOT USE    RD  WR        RD      WR       RD     WR    RD  WR        RD      WR       RD     WR\n");
		}else{
		//                vbd: 123,12345: (123,123) (1234.56,1234.56) (1234.56,1234.56) (123.45,123.45)
		fprintf(stdout, "      DOM   VBD  BLKBKRING     BLKTAP REQS/s    TAPDISK REQS/s     TAPDISK MB/s\n");
		fprintf(stdout, "                  TOT USE        RD      WR        RD      WR       RD     WR\n");
		}
#else
		//                vbd: 123,12345: (123,123) (123,123) (1234.56,1234.56) (123.45,123.45)
		fprintf(stdout, "      DOM   VBD  BLKBKRING  INFLIGHT     BLKTAP REQS/s     BLKTAP MB/s\n");
		fprintf(stdout, "                  TOT USE    RD  WR        RD      WR       RD     WR\n");
#endif	/* MT_USE_TAPDISK */

		// Loop through all VBDs
		vbd = mpool->vbds;
		while (vbd){

			// Update vbd statistics
#ifdef	MT_USE_TAPDISK
			if (vbd_bb_update(vbd) ||
			    vbd_bt_update(vbd) ||
			    vbd_td_update(vbd)){
				quit = 1;
				return;
			}
#else
			if (vbd_bb_update(vbd) ||
			    vbd_bt_update(vbd)){
				quit = 1;
				return;
			}
#endif	/* MT_USE_TAPDISK */

			// Print general VBD info
			fprintf(stdout, " vbd: %3d,%5d:", vbd->domid, vbd->vbdid);

			// Print blkbk io_ring stats
			fprintf(stdout, " (%3u,%3u)", vbd->bbstat.iorsz, vbd->bbstat.ioreq-vbd->bbstat.iorsp);

			// Print blktap inflight stats
#ifdef	MT_USE_TAPDISK
			if (extended){
				fprintf(stdout, " (%3u,%3u)", vbd->btstat.infrd, vbd->btstat.infwr);
			}
#else
			fprintf(stdout, " (%3u,%3u)", vbd->btstat.infrd, vbd->btstat.infwr);
#endif	/* MT_USE_TAPDISK */

			// Print blktap iops
			fprintf(stdout, " (%7.2f,", ((float)(vbd->btstat.rds_0-vbd->btstat.rds_1))/now_diff);
			fprintf(stdout, "%7.2f)",   ((float)(vbd->btstat.wrs_0-vbd->btstat.wrs_1))/now_diff);

			// Print blktap rw throughput
#ifdef	MT_USE_TAPDISK
			if (extended){
				fprintf(stdout, " (%6.2f,", (((float)(vbd->btstat.rsc_0-vbd->btstat.rsc_1)*MT_SECTOR_SZ)/(float)unit)/now_diff);
				fprintf(stdout, "%6.2f)",   (((float)(vbd->btstat.wsc_0-vbd->btstat.wsc_1)*MT_SECTOR_SZ)/(float)unit)/now_diff);
			}
#else
			fprintf(stdout, " (%6.2f,", (((float)(vbd->btstat.rsc_0-vbd->btstat.rsc_1)*MT_SECTOR_SZ)/(float)unit)/now_diff);
			fprintf(stdout, "%6.2f)",   (((float)(vbd->btstat.wsc_0-vbd->btstat.wsc_1)*MT_SECTOR_SZ)/(float)unit)/now_diff);
#endif	/* MT_USE_TAPDISK */

#ifdef	MT_USE_TAPDISK
			// Print tapdisk inflight
			if (extended){
				fprintf(stdout, " (%3u,%3u)", vbd->tdstat.infrd, vbd->tdstat.infwr);
			}

			// Print tapdisk iops
			fprintf(stdout, " (%7.2f,", ((float)(vbd->tdstat.rds_0-vbd->tdstat.rds_1))/now_diff);
			fprintf(stdout, "%7.2f)",   ((float)(vbd->tdstat.wrs_0-vbd->tdstat.wrs_1))/now_diff);

			// print tapdisk rw throughput
			fprintf(stdout, " (%6.2f,", (((float)(vbd->tdstat.rsc_0-vbd->tdstat.rsc_1)*MT_SECTOR_SZ)/(float)unit)/now_diff);
			fprintf(stdout, "%6.2f)",   (((float)(vbd->tdstat.wsc_0-vbd->tdstat.wsc_1)*MT_SECTOR_SZ)/(float)unit)/now_diff);
#endif	/* MT_USE_TAPDISK */

			// iterate to the next vbd
			fprintf(stdout, "\n");
			vbd = vbd->next;
		}

		// Move to next mpool in list
		fprintf(stdout, "\n");
		mpool = mpool->next;
	}
	// Flush in case the output is being piped
	fflush(stdout);
}

// Main
int main(int argc, char **argv) {
	// Local variables
	int32_t		domid = -1;		// Domain ID
	int32_t		vbdid = -1;		// VBD ID
	int32_t		inter = -1;		// Dump interval

	struct itimerval	itv;		// itimer setup

	char		*datafn = NULL;		// datafile pathname
	FILE		*datafp = NULL;		// datafile file pointer

	int		i;			// temporary integer

	int		err = 0;		// return value

	// Fetch arguments

#ifdef	MT_USE_TAPDISK
	while ((i = getopt(argc, argv, "hxd:v:i:o:")) != -1){
#else
	while ((i = getopt(argc, argv, "hd:v:i:o:")) != -1){
#endif	/* MT_USE_TAPDISK */

		switch (i){

#ifdef	MT_USE_TAPDISK
			case 'x':
				// Set extended stats, if unset
				if (extended != 0){
					fprintf(stderr, "%s: Invalid argument \"-x\", extended output already set.\n", argv[0]);
					goto err;
				}
				extended++;
				break;
#endif	/* MT_USE_TAPDISK */

			case 'd':
				// Set domain_id, if unset
				if (domid != -1){
					fprintf(stderr, "%s: Invalid argument \"-d\", Domain ID already set.\n", argv[0]);
					goto err;
				}
				domid = (int32_t)strtoul(optarg, NULL, 10);
				break;

			case 'v':
				// Set VBD ID, if unset
				if (vbdid != -1){
					fprintf(stderr, "%s: Invalid argument \"-v\", VBD ID already set.\n", argv[0]);
					goto err;
				}
				vbdid = (int32_t)strtoul(optarg, NULL, 10);
				break;

			case 'i':
				// Set output intervals, if unset
				if (inter != -1){
					fprintf(stderr, "%s: Invalid argument \"-i\", output interval already set.\n", argv[0]);
					goto err;
				}
				inter = (int32_t)strtoul(optarg, NULL, 10);
				break;

			case 'o':
				// Set datafile name
				if (datafn != NULL){
					fprintf(stderr, "%s: Invalid argument \"-o\", output datafile already set.\n", argv[0]);
					goto err;
				}
				if ((datafn = strdup(optarg)) == NULL){
					perror("strdup");
					fprintf(stderr, "%s: Error allocating memory for datafile name.\n", argv[0]);
					goto err;
				}
				break;

			case 'h':
			default:
				// Print help
				usage(argv[0]);
				goto out;
		}
	}

	// Validate parameters and set defaults
	if ((domid < 0) && (vbdid >= 0)){
		fprintf(stderr, "%s: Error. If VBD (-v) is specified, DOM (-d) needs to be specified too.\n\n", argv[0]);
		usage(argv[0]);
		goto out;
	}
	if (inter < 0){
		inter = MT_INTERVAL;
	}
	if (datafn != NULL){
		if ((datafp = fopen(datafn, "w")) == NULL){
			perror("fopen");
			fprintf(stderr, "%s: Error opening datafile '%s' for writing.\n", argv[0], datafn);
			goto err;
		}
	}
	memset(&now_0, 0, sizeof(now_0));
	memset(&now_1, 0, sizeof(now_1));

	// Associate currently attached VBDs to their mempools
	if (vbds_alloc(&mpools, domid, vbdid) != 0){
		goto err;
	}
	gettimeofday(&now_0, NULL);

	// Set alarm
	signal(SIGALRM, sigalarm_h);
	itv.it_interval.tv_sec = inter/1000;
	itv.it_interval.tv_usec = (inter%1000)*1000;
	itv.it_value.tv_sec = inter/1000;
	itv.it_value.tv_usec = (inter%1000)*1000;
	setitimer(ITIMER_REAL, &itv, NULL);

	// Loop until an error condition (e.g. a vbd was detached) has occurred.
	while(!quit){
		pause();
		main_loop();
	}

	// Skip error section
	goto out;

err:
	err = 1;

out:
	// Release global resources (mpool and vbd list)
	// TODO: ...

	// Release local resources
	if (datafn)
		free(datafn);
	if (datafp)
		fclose(datafp);

	// Return
	return(err);
}
