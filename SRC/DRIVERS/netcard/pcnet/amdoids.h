/*++

Copyright (c) 1997 ADVANCED MICRO DEVICES, INC. All Rights Reserved.
This software is unpblished and contains the trade secrets and 
confidential proprietary information of AMD. Unless otherwise provided
in the Software Agreement associated herewith, it is licensed in confidence
"AS IS" and is not to be reproduced in whole or part by any means except
for backup. Use, duplication, or disclosure by the Government is subject
to the restrictions in paragraph (b) (3) (B) of the Rights in Technical
Data and Computer Software clause in DFAR 52.227-7013 (a) (Oct 1988).
Software owned by Advanced Micro Devices, Inc., One AMD Place,
Sunnyvale, CA 94088.

Module Name:

	amdoids.h

Abstract:

	Header file containing custom NDIS4 OID definitions for AMD PcNet hardware

Environment:

Created:

	10/10/97

Revision History:

	$Log:   V:/pvcs/archives/network/pcnet/homelan/wince/mini4/src/amdoids.h_y  $
 * 
 *    Rev 1.0   Apr 07 1999 18:13:16   jagannak
 * Initial check-in
 * 
 *    Rev 1.9   Mar 31 1999 12:08:26   cabansag
 *  
 * 
 *    Rev 1.0   06 Jan 1998 11:47:06   steiger
 *  

Author(s):	Mike Steiger a.k.a. Rich Compelling
--*/

#ifndef _LANCEOID_
#define _LANCEOID_

#define	AMD_LANGUAGE_ENGLISH			1
#define	AMD_LANGUAGE_GERMAN				2
#define	AMD_LANGUAGE_FRENCH				3
#define	AMD_LANGUAGE_ITALIAN			4
#define	AMD_LANGUAGE_SPANISH			5

/* Select driver language reported to DMI here */
/* This does not change the language of driver messages */

#define	MY_LANGUAGE						AMD_LANGUAGE_ENGLISH

/* Custom OIDs for DMI 2.0 support */

/* Implementation specfic OID type for operations */
#define	OID_TYPE_CUSTOM_OPER					0xFF010000
#define	OID_TYPE_OPTIONAL						0x00000200

/* Custom OIDs for DMI 2.0 */
#define	OID_CUSTOM_DMI_CI_INFO			(OID_TYPE_CUSTOM_OPER | OID_TYPE_OPTIONAL | 0x00)
#define	OID_CUSTOM_DMI_CI_INIT			(OID_TYPE_CUSTOM_OPER | OID_TYPE_OPTIONAL | 0xFF)

#define	DMI_OPCODE_GET_PERM_ADDRESS		1
#define	DMI_OPCODE_GET_CURR_ADDRESS		2
#define	DMI_OPCODE_GET_TOTAL_TX_PKTS	3
#define	DMI_OPCODE_GET_TOTAL_TX_BYTES	4
#define	DMI_OPCODE_GET_TOTAL_RX_PKTS	5
#define	DMI_OPCODE_GET_TOTAL_RX_BYTES	6
#define	DMI_OPCODE_GET_TOTAL_TX_ERRS	7
#define	DMI_OPCODE_GET_TOTAL_RX_ERRS	8
#define	DMI_OPCODE_GET_TOTAL_HOST_ERRS	9
#define	DMI_OPCODE_GET_TOTAL_WIRE_ERRS	10
#define	DMI_OPCODE_GET_MULTI_ADDRESS	11
#define	DMI_OPCODE_GET_DRIVER_VERSION	12
#define	DMI_OPCODE_GET_DRIVER_NAME		13
#define	DMI_OPCODE_GET_TX_DUPLEX		14
#define	DMI_OPCODE_GET_OP_STATE			15
#define	DMI_OPCODE_GET_USAGE_STATE		16
#define	DMI_OPCODE_GET_AVAIL_STATE		17
#define	DMI_OPCODE_GET_ADMIN_STATE		18
#define	DMI_OPCODE_GET_FATAL_ERR_COUNT	19
#define	DMI_OPCODE_GET_MAJ_ERR_COUNT	20
#define	DMI_OPCODE_GET_WRN_ERR_COUNT	21
#define	DMI_OPCODE_GET_BOARD_FOUND		22
#define	DMI_OPCODE_GET_IO_BASE_ADDR		23
#define	DMI_OPCODE_GET_GET_IRQ			24
#define	DMI_OPCODE_GET_DMA_CHANNEL		25
#define	DMI_OPCODE_GET_CSR_NUMBER		26
#define	DMI_OPCODE_GET_CSR_VALUE		27
#define	DMI_OPCODE_GET_BCR_NUMBER		28
#define	DMI_OPCODE_GET_BCR_VALUE		29
#define	DMI_OPCODE_SET_CSR_NUMBER		30
#define	DMI_OPCODE_SET_CSR_VALUE		31
#define	DMI_OPCODE_SET_BCR_NUMBER		32
#define	DMI_OPCODE_SET_BCR_VALUE		33
#define	DMI_OPCODE_GET_CSR0_ERR			34
#define	DMI_OPCODE_GET_CSR0_BABL		35
#define	DMI_OPCODE_GET_CSR0_CERR		36
#define	DMI_OPCODE_GET_CSR0_MISS		37
#define	DMI_OPCODE_GET_CSR0_MERR		38
#define	DMI_OPCODE_GET_TX_DESC_ERR		39
#define	DMI_OPCODE_GET_TX_DESC_MORE		40
#define	DMI_OPCODE_GET_TX_DESC_ONE		41
#define	DMI_OPCODE_GET_TX_DESC_DEF		42
#define	DMI_OPCODE_GET_TX_DESC_BUF		43
#define	DMI_OPCODE_GET_TX_DESC_UFLO		44
#define	DMI_OPCODE_GET_TX_DESC_EXDEF	45
#define	DMI_OPCODE_GET_TX_DESC_LCOL		46
#define	DMI_OPCODE_GET_TX_DESC_LCAR		47
#define	DMI_OPCODE_GET_TX_DESC_RTRY		48
#define	DMI_OPCODE_GET_RX_DESC_ERR		49
#define	DMI_OPCODE_GET_RX_DESC_FRAM		50
#define	DMI_OPCODE_GET_RX_DESC_OFLO		51
#define	DMI_OPCODE_GET_RX_DESC_CRC		52
#define	DMI_OPCODE_GET_RX_DESC_BUF		53
#define	DMI_OPCODE_GET_LANGUAGE			54
//#define	DMI_OPCODE_GET_DRIVER_SIZE		55
#define	DMI_OPCODE_GET_STATUS			56
#define	DMI_OPCODE_GET_NET_TOP			57
#define	DMI_OPCODE_GET_RAM_SIZE			58


//#define	DMI_OPCODE_GET_

/* Defines for DMI_OPCODE_GET_BOARD_FOUND */

#define	DMI_NO_BOARD					0
#define	DMI_PCI_BOARD					1
#define	DMI_PLUG_PLAY_BOARD				2
#define	DMI_LOCAL_BOARD					3
#define	DMI_ISA_BOARD					4

#define	OID_DATA_SIZE					64
#endif /* _LANCEOID_ */
