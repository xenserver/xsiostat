xsiostat - XenServer Storage I/O Stats
======================================

xsiostat is a tool to expose metrics of the XenServer storage subsystem
on a per-VBD basis.

Specific features of xsiostat include

*    Providing information about each VBD in terms of:
  *   Total pages available and in use on the corresponding page pool
  *   Blkfront/Blkback I/O ring utilisation
  *   Number of inflight read and write requests
  *   Number of requests processed per second by blktap2
  *   Read and write throughput in MB/s
*    Enabling filtering by domain and by VBD
*    Print statistics grouped by page pool

Quick Start
===========

Prerequisites to build
----------------------

*   GNU C compiler (gcc)
*   GNU C library (libc)
*   GNU make utility (make)

Commands to build
-----------------

    git clone http://github.com/xenserver/xsiostat
    cd xsiostat
    make

Runtime Dependencies
--------------------

For the time being, xsiostat is only compatible with XenServer environments.
It requires all running guests to have block devices plugged via PV drivers.
It relies on specific entries to be available on sysfs, some of which were
only introduced in XenServer 6.1.0. It will fail to run on earlier releases.
Currently, plugging more VBDs or unplugging currently attached VBDs will
cause xsiostat to either produce incomplete information or to quit.
