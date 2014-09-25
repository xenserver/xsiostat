xsiostat - XenServer Storage I/O Stats
======================================

xsiostat is a tool to expose metrics of the XenServer storage subsystem
on a per-VBD basis.

Specific features of xsiostat include

*    Providing information about each VBD in terms of:
  *   Number of read and write operations completed per second
  *   Read and write throughput (in MB/s)
  *   Average queue size for reads and writes
*    Enabling filtering by domain and by VBD

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

At this time, xsiostat is only compatible with XenServer 6.5 environments.

Known Issues
------------

* Consider requests merged
* Finish the data output code (writing to a datafile does nothing atm)
* Implement datafile read code (there is no way to read a datafile)
