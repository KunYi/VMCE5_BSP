Readme.txt
==========
AMD PCnet-Home Software
NDIS4 Miniport Driver for Windows CE


Release 1.0
============

The included Windows CE NDIS 4.0 Miniport driver and associated files are part
of the PCnet-Home DMA Busmaster NDIS 4.0 Miniport driver for Windows CE. This driver has been 
tested under the Windows CE Platform Builder Version 2.11(OEM version). 
Please note: You need to rebuild the driver in order to load it in your Windows CE environment.
	     The build instructions are in the Release.txt file as described below. 

Contents
========
2 directories - SRC and DOCS in the current directory. 

1. SRC subdirectory - Contains all the source files for the Windows CE NDIS driver. 

   alloc.c      -- Memory and data structure allocation modules.
   interrup.c   -- ISR, DPC(send and receive isr) modules.
   lance.c      -- Init, register, stop, start, and scan controller modules.
   request.c    -- Ndis OID set and query modules.
   send.c       -- packet send module.
   wince.c      -- installation information for the Windows CE driver. 
   lancehrd.h   -- Hardware related header file.
   lancesft.h   -- Software related header file.
   amddmi.h     --
   amdoids.h    --
   binsig.h     --
   lance.msg    --
   makefile     -- Makefile that called by nmake.
   makefile.inc -- 
   sources      -- build utility input file.
   pcntn4m.def  -- Export functions defined in this file. 
   pcntn4m.mc   --
   pcntn3.rc    --    

2. DOCS subdirectory - Contains all the documentation for build and installation of the driver. 
   Release.txt  -- Has build information of the driver. 
    
Windows CE NDIS 4.0 Miniport driver has been tested with the following devices:
===============================================================================

1. PCnet-Home NIC. 


To build PCnet-Home driver, PCNTN4M.DLL, NDIS4 miniport:
======================================================== 
Please refer to the Release.txt file for a detailed description of the build process. 

 
    

 