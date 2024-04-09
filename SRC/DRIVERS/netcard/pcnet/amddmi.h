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

	amddmi.h

Abstract:

	Header file for PCNET.DLL, a DMI CI module for win32 NDIS environment.

Environment:

	NDIS4, uses OID interface into driver with custom OIDs to communicate
	with the NDIS driver.

Created:

	11/04/97

Revision History:

	$Log:   V:/pvcs/archives/network/pcnet/homelan/wince/mini4/src/amddmi.h_y  $
 * 
 *    Rev 1.0   Apr 07 1999 18:13:08   jagannak
 * Initial check-in
 * 
 *    Rev 1.9   Mar 31 1999 12:08:24   cabansag
 *  
 * 
 *    Rev 1.0   06 Jan 1998 11:47:04   steiger
 *  

--*/

#if !defined(AMDDMI_H)
#define AMDDMI_H

/***************************************************************************/
/*********************** OS Specific Definitions ***************************/
/***************************************************************************/
#define	DMI_API					WINAPI

#define DLLENTRY __declspec(dllexport)
#define STDENTRY DLLENTRY HRESULT WINAPI
#define STDENTRY_(type) DLLENTRY type WINAPI

/***************************************************************************/
/******************* Definitions used in structures ************************/
/***************************************************************************/

typedef unsigned char			DMI_BYTE;
typedef signed long int			DMI_INT;
typedef unsigned long int		DMI_OFFSET;
typedef unsigned long int		DMI_UNSIGNED;

/***************************************************************************/
/************************** Miscellaneous Constants ************************/
/***************************************************************************/
/* Constants for DMI_OPCODE_GET_STATUS call to driver */

#define	REDUNDANT_STATE			0
#define	LINK_STATE				1

#define	MAJOR_VERS				2
#define	MINOR_VERS				1
#define	BUILD_VERS				0

#define	DMI_REQ_FAILURE			-1
#define	DMI_REQ_SUCCESS			0

#define	LANCE_PORT_FAILURE		DMI_REQ_FAILURE
#define	LANCE_PORT_SUCCESS		DMI_REQ_SUCCESS

#define MAC_ADDR_LENGTH			6
#define	CURR_ATTRIBUTE			0
#define	NEXT_ATTRIBUTE			1

#ifndef WORD
 #define	WORD	USHORT
#endif

#ifndef BYTE
 #define	BYTE	UCHAR
#endif

#ifndef DWORD
 #define	DWORD	ULONG
#endif

#ifndef MAX_STRING_LENGTH
 #define	MAX_STRING_LENGTH	32
#endif

#ifndef BOOLEAN
 #define	BOOLEAN	UCHAR
#endif

/***************************************************************************/
/*********************************** ENUMs *********************************/
/***************************************************************************/

typedef enum DmiSetMode
{
	DMI_SET,
	DMI_RESERVE,
	DMI_RELEASE
}DmiSetMode_t;

typedef enum DmiRequestMode
{	
	DMI_UNIQUE,
	DMI_FIRST,
	DMI_NEXT
}DmiRequestMode_t;

typedef enum DmiStorageType
{
	MIF_COMMON,
	MIF_SPECIFIC
}DmiStorageType_t;

typedef enum DmiAccessMode
{
	MIF_UNKNOWN_ACCESS,
	MIF_READ_ONLY,
	MIF_READ_WRITE,
	MIF_WRITE_ONLY,
	MIF_UNSUPPORTED
}DmiAccessMode_t;

typedef enum DmiDataType
{
	MIF_DATATYPE_0,
	MIF_COUNTER,
	MIF_COUNTER64,
	MIF_GAUGE,
	MIF_DATATYPE_4,
	MIF_INTEGER,
	MIF_INTEGER64,
	MIF_OCTETSTRING,
	MIF_DISPLAYSTRING,
	MIF_DATATYPE_9,
	MIF_DATATYPE_10,
	MIF_DATE
}DmiDataType_t;

typedef enum DmiFileType
{
	DMI_FILETYPE_0,
	DMI_FILETYPE_1,
	DMI_MIF_FILE_NAME,
	DMI_MIF_FILE_DATA,
	SNMP_MAPPING_FILE_NAME,
	SNMP_MAPPING_FILE_DATA,
	DMI_GROUP_FILE_NAME,
	DMI_GROUP_FILE_DATA,
	VENDOR_FORMAT_FILE_NAME,
	VENDOR_FORMAT_FILE_DATA,
}DmiFileType_t;

/***************************************************************************/
/********************************** typedefs *******************************/
/***************************************************************************/
typedef void				DmiVoid_t;
typedef unsigned __int64	OsDmiCounter64_t;
typedef _int64				OsDmiInteger64_t;
typedef OsDmiCounter64_t	DmiCounter64_t;
typedef OsDmiInteger64_t	DmiInteger64_t;

typedef unsigned long		DmiId_t;
typedef unsigned long		DmiHandle_t;
typedef unsigned long		DmiCounter_t;
typedef unsigned long		DmiErrorStatus_t;
typedef unsigned long		DmiGauge_t;
typedef unsigned long		DmiUnsigned_t;
typedef unsigned char		DmiBoolean_t;
typedef long				DmiInteger_t;

/***************************************************************************/
/********************************* Structures ******************************/
/***************************************************************************/

/* DMI Request Block structure */
typedef struct ReqBlock
{
	WORD    			Opcode;
	WORD    			Status;
	BYTE    			addr[8];
	union
	{
		DWORD  			Value;
		DmiCounter64_t	Counter;
	};
}DMIREQBLOCK;

typedef struct  DmiTimestamp
{
	unsigned char		year[ 4 ];
	unsigned char		month[ 2 ];
	unsigned char		day[ 2 ];
	unsigned char		hour[ 2 ];
	unsigned char		minutes[ 2 ];
	unsigned char		seconds[ 2 ];
	unsigned char		dot;
	unsigned char		microSeconds[ 6 ];
	unsigned char		plusOrMinus;
	unsigned char		utcOffset[ 3 ];
	unsigned char		padding[ 3 ];
}DmiTimestamp_t;

typedef struct  DmiString
{
	DmiUnsigned_t		size;
	unsigned char *		body;
}DmiOctetString_t, DmiString_t, *DmiStringPtr_t;

//typedef struct  DmiOctetString
//{
//	DmiUnsigned_t		size;
//	unsigned char *		body;
//}DmiOctetString_t;

typedef struct  DmiDataUnion
{
	DmiDataType_t		type;
	union 
	{
		DmiCounter_t		counter;
		DmiCounter64_t		counter64;
		DmiGauge_t			gauge;
		DmiInteger_t		integer;
		DmiInteger64_t		integer64;
		DmiOctetString_t *	octetstring;
		DmiString_t *		string;
		DmiTimestamp_t *	date;
	}value;
}DmiDataUnion_t;

typedef struct  DmiEnumInfo
{
	DmiString_t *		name;
	DmiInteger_t		value;
}DmiEnumInfo_t;

typedef struct  DmiAttributeInfo
{
	DmiId_t					id;
	DmiString_t *			name;
	DmiString_t *			pragma;
	DmiString_t *			description;
	DmiStorageType_t		storage;
	DmiAccessMode_t			access;
	DmiDataType_t			type;
	DmiUnsigned_t			maxSize;
	struct DmiEnumList *	enumList;
}DmiAttributeInfo_t;

typedef struct  DmiAttributeData
{
	DmiId_t				id;
	DmiDataUnion_t		data;
}DmiAttributeData_t;

typedef struct  DmiGroupInfo
{
	DmiId_t						id;
	DmiString_t *				name;
	DmiString_t *				pragma;
	DmiString_t *				className;
	DmiString_t *				description;
	struct DmiAttributeIds *	keyList;
}	DmiGroupInfo_t;

typedef struct  DmiComponentInfo
{
	   DmiId_t			id;
	   DmiString_t *	name;
	   DmiString_t *	pragma;
	   DmiString_t *	description;
	   DmiBoolean_t		exactMatch;
}DmiComponentInfo_t;

typedef struct  DmiFileDataInfo
{
	DmiFileType_t		fileType;
	DmiOctetString_t *	fileData;
}DmiFileDataInfo_t;

typedef struct  DmiClassNameInfo 
{
	DmiId_t				id;
	DmiString_t *		className;
}DmiClassNameInfo_t;

typedef struct  DmiRowRequest
{
	DmiId_t						compId;
	DmiId_t						groupId;
	DmiRequestMode_t			requestMode;
	struct DmiAttributeValues *	keyList;
	struct DmiAttributeIds *	ids;
}DmiRowRequest_t;

typedef struct  DmiRowData
{
	DmiId_t						compId;
	DmiId_t						groupId;
	DmiString_t *				className;
	struct DmiAttributeValues *	keyList;
	struct DmiAttributeValues *	values;
}DmiRowData_t;

typedef struct  DmiAttributeIds
{
	DmiUnsigned_t			size;
	DmiId_t *				list;
}DmiAttributeIds_t;

typedef struct  DmiAttributeValues
{
	DmiUnsigned_t			size;
	DmiAttributeData_t *	list;
}DmiAttributeValues_t;

typedef struct  DmiEnumList
{
	DmiUnsigned_t			size;
	DmiEnumInfo_t *			list;
}DmiEnumList_t;

typedef struct  DmiAttributeList
{
	DmiUnsigned_t			size;
	DmiAttributeInfo_t *	list;
}DmiAttributeList_t;

typedef struct  DmiGroupList
{
	DmiUnsigned_t			size;
	DmiGroupInfo_t *		list;
}DmiGroupList_t;

typedef struct  DmiComponentList
{
	DmiUnsigned_t			size;
	DmiComponentInfo_t *	list;
}	DmiComponentList_t;

typedef struct  DmiFileDataList
{
	DmiUnsigned_t			size;
	DmiFileDataInfo_t *		list;
}DmiFileDataList_t;

typedef struct  DmiClassNameList
{
	DmiUnsigned_t			size;
	DmiClassNameInfo_t *	list;
}DmiClassNameList_t;

typedef struct  DmiStringList
{
	DmiUnsigned_t			size;
	DmiStringPtr_t *		list;
}DmiStringList_t;

typedef struct  DmiFileTypeList
{
	DmiUnsigned_t			size;
	DmiFileType_t *			list;
}DmiFileTypeList_t;

typedef struct  DmiMultiRowRequest
{
	DmiUnsigned_t			size;
	DmiRowRequest_t *		list;
}DmiMultiRowRequest_t;

typedef struct  DmiMultiRowData
{
	DmiUnsigned_t			size;
	DmiRowData_t *			list;
}DmiMultiRowData_t;

typedef struct  DmiNodeAddress
{
	DmiString_t *			address;
	DmiString_t *			rpc;
	DmiString_t *			transport;
}DmiNodeAddress_t;

#endif // AMDDMI_H
