/*****************************************************************************
 *
 * Copyright (c) 1997 ADVANCED MICRO DEVICES, INC. All Rights reserved.
 *
 * This software is unpublished and contains the trade secrets and 
 * confidential proprietary information of AMD. Unless otherwise
 * provided in the Software Agreement associated herewith, it is
 * licensed in confidence "AS IS" and is not to be reproduced in
 * whole or part by any means except for backup. Use, duplication,
 * or disclosure by the Government is subject to the restrictions
 * in paragraph(b)(3)(B)of the Rights in Technical Data and
 * Computer Software clause in DFAR 52.227-7013(a)(Oct 1988).
 * Software owned by Advanced Micro Devices Inc., One AMD Place
 * P.O. Box 3453, Sunnyvale, CA 94088-3453.
 *
 *****************************************************************************
 *
 * BINSIG.H
 *
 * This C format header file contains the definition of a binary signature
 * structure to be included in all object code delivered by SSE.
 *
 * The purpose of this signature is to enable access to key information
 * for traceability of binary files. In addition, the uniform structure
 * will enable OEM branding of signon and copyright messages without the
 * need to have access to source code. Both functions will be achieved
 * by a separate utility which will scan for, read, and optionally write to
 * fields within the signature. Checksum fields are provided to enable the
 * provision of a post-compilation checksum for distribution integrity, as
 * well as provide the means to correct existing checksums after binary
 * editing. 
 *
 * Written by:   David Balmforth and Linh Tran
 * Date:         February 1997
 *
 *****************************************************************************
 * Modification History
 *
 * $Log:   V:/pvcs/archives/network/pcnet/homelan/wince/mini4/src/binsig.h_y  $
 * 
 *    Rev 1.0   Apr 07 1999 18:13:22   jagannak
 * Initial check-in
 * 
 *    Rev 1.9   Mar 31 1999 12:08:26   cabansag
 *  
 * 
 *    Rev 1.57   01 Oct 1997 18:45:32   steiger
 * Changed version to 1.01.006
 * 
 *    Rev 1.56   30 Sep 1997 15:15:20   steiger
 * Changed version to 1.01.005
 * Changed all instances of PCNTN4M to PCNTN3M when appropriate
 * 
 *    Rev 1.55   02 Sep 1997 14:06:20   steiger
 * Changed version to 1.01.004.
 * 
 *    Rev 1.54   20 Aug 1997 14:23:46   steiger
 * Further refinements to multi-send and multi-receive.
 * 
 * 
 *    Rev 1.53   30 Jun 1997 18:25:28   steiger
 * Changed version to 1.01.002
 * 
 *    Rev 1.52   08 May 1997 10:13:14   steiger
 * Changed from general relase to beta release.
 * 
 *    Rev 1.51   07 May 1997 18:43:32   steiger
 * Added Redundant mode to driver.
 *
 * Mod No  Date    Who         Description 
 *   00   3/06/97  HL		Declare the type name for the structure
 *   01	 3/08/97   RV		Commented out the instantiation of the structure
 *							but completed it as an example.
 *
 *****************************************************************************/
#define AMD_INTERNAL    0x0001000
#define AMD_PREALPHA    0x0002000
#define AMD_ALPHA       0x0004000
#define AMD_BETA        0x0008000
#define AMD_RELEASECAN  0x0010000
#define AMD_GENERALREL  0x0200000

typedef	struct	DRIVERINFO_TAG {
	char   			MarkBegin[12]; /* Unique ID used by the tool*/
	unsigned long	StructureRev; 	/* Rev of struct for future add-in*/
								  			/* features flexibility*/
								  			/* (use by AMD utility)*/
	unsigned long	OSChksumFlag; 	/* Flag if OS will do checksum*/
	unsigned long	AMDChksum;	  	/* To be assigned by AMD utility*/
								  			/* to 2's complement of the*/
								  			/* binary's sum before the*/
								  			/* release if OSChksumFlag is not*/
								  			/* set(Target OS does not do checksum)*/
	unsigned long	ChksumCorrection;		/* To be assigned by AMD utility*/
									 		/* to 2's complement of the*/
									 		/* binary's sum after the binary*/
									 		/* has been edited (assume*/
									 		/* binary's sum = 0 means pass)*/
	char			DriverName[16];	/* i.e. PCNTNW.LAN*/
	char			DriverVersion[12]; /* "x.yy.zzz"*/
	char			Date[12];		 	/* "mm-dd-yyyy" - date the driver*/
										 	/* was built*/
	unsigned long	Status;			/* I-internal, A-alpha, B-beta*/
											/* release etc...*/
	char		   	CopyrightString[44];	/* (C)1996 - 1997 by Advanced*/
											/* Micro Devices.*/
	char  			SignOnMessage[128];		/* */
	char  			Reserved[4];	/* */
	char  			MarkEnd[8];	 	/* Unique ID used by the tool*/
} DRIVERINFO_TYPE;


/*****************************************************************************

 Sample instantiation for the structure.  Copy this to somewhere
 in your driver and fill in as appropriate.  Pay attention to string
 lengths!  Its good practice to leave at least one less character
 then the string length for the /0 terminator otherwise you'll have
 a non-terminated string.

struct DRIVERINFO_TAG
DRIVERINFO = {"AMD_START   ",0x01,0,0,0,
				  "MYDRVR.LAN","1.00.000","01-01-1997",0,
				  " (C)1996 - 1997 by Advanced Micro Devices. ",
				  "This is where you can put your cool sign on message if you want to.",
				  ""," AMD_END"};

*****************************************************************************/

DRIVERINFO_TYPE DRIVERINFO =
{
   "AMD_START   ",
   0x01,
   1,
   0,
   0,
#ifdef NDIS40_MINIPORT
	"PCNTN4HL.SYS",
#elif defined(NDIS50_MINIPORT)
   "PCNTN5HL.SYS",
#else
   "PCNTN3HL.SYS",
#endif
   "1.03",
   "03-02-1998",
   AMD_GENERALREL,
   " (C)1996 - 1999 by Advanced Micro Devices. ",
   "",
   "",
   " AMD_END"
};
