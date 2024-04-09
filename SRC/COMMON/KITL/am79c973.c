//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
#include <windows.h>
#include <halether.h>

//------------------------------------------------------------------------------

#define Log                EdbgOutputDebugString

//------------------------------------------------------------------------------

#define BUFFER_SIZE        0x0600
#define DESC_SIZE          0x10
#define INIT_SIZE          0x20
#define ADDR_SIZE          6

#define TIMEOUT            2

#define RX_BUFFERS         32
#define TX_BUFFERS         4

//------------------------------------------------------------------------------

#define APROM              0x00
#define RDP                0x10
#define RAP                0x14
#define RESET              0x18
#define BDP                0x1C

//------------------------------------------------------------------------------

#define RMD1_OWN           0x80000000
#define RMD1_ERR           0x40000000
#define RMD1_FRAM          0x20000000
#define RMD1_OFLO          0x10000000
#define RMD1_CRC           0x08000000
#define RMD1_BUFF          0x04000000
#define RMD1_STP           0x02000000
#define RMD1_ENP           0x01000000
#define RMD1_BPE           0x00800000
#define RMD1_ONES          0x0000F000

#define TMD0_BUFF          0x80000000
#define TMD0_UFLO          0x40000000
#define TMD0_EXDEF         0x20000000
#define TMD0_LCOL          0x10000000
#define TMD0_LCAR          0x08000000
#define TMD0_RTRY          0x04000000

#define TMD1_OWN           0x80000000
#define TMD1_ERR           0x40000000
#define TMD1_ADD_FCS       0x20000000
#define TMD1_MORE          0x10000000
#define TMD1_LTINT         0x10000000
#define TMD1_ONE           0x08000000
#define TMD1_DEF           0x04000000
#define TMD1_STP           0x02000000
#define TMD1_ENP           0x01000000
#define TMD1_BPE           0x00800000
#define TMD1_ONES          0x0000F000

//------------------------------------------------------------------------------

#define INP32(x)           (*(volatile DWORD *)(x))
#define OUT32(x, y)        *(volatile DWORD *)(x)=(y)

#define V2P(a)             ((DWORD)(a) & ~0xE0000000)
#define C2U(a)             ((DWORD)(a) | 0xA0000000)

//------------------------------------------------------------------------------

static DWORD g_base = 0;               // The chip base address
static DWORD g_dmaAddress = 0;         // DMA buffer address
static ULONG g_dmaSize = 0;            // DMA buffer size

static ULONG *g_pInit;
static ULONG *g_pRxRing;
static ULONG *g_pTxRing;
static UCHAR *g_pRxBuffer;
static UCHAR *g_pTxBuffer;

static ULONG g_rxPos;
static ULONG g_txPos;

//------------------------------------------------------------------------------

static void  OEMSleep(DWORD delay)
{
	//	Simply loop...
	while (delay--)
		;
}

//------------------------------------------------------------------------------

static ULONG ReadCSR(DWORD address)
{
   OUT32(g_base + RAP, address);
   return INP32(g_base + RDP);
}

//------------------------------------------------------------------------------

static void WriteCSR(DWORD address, DWORD data)
{
   OUT32(g_base + RAP, address);
   OUT32(g_base + RDP, data);
}

//------------------------------------------------------------------------------

static ULONG ReadBCR(DWORD address)
{
   OUT32(g_base + RAP, address);
   return INP32(g_base + BDP);
}

//------------------------------------------------------------------------------

static void WriteBCR(DWORD address, DWORD data)
{
   OUT32(g_base + RAP, address);
   OUT32(g_base + BDP, data);
}

//------------------------------------------------------------------------------

static USHORT ReadMII(DWORD address)
{
   WriteBCR(33, (0x1e << 5) | (address & 0x1F));
   return (USHORT)ReadBCR(34);
}

//------------------------------------------------------------------------------

static void WriteMII(ULONG address, USHORT data)
{
   WriteBCR(33, (0x1e<<5) | (address & 0x1F));
   WriteBCR(34, data);
}

//------------------------------------------------------------------------------

static DWORD HashAddress(UCHAR* pAddress)
{
   ULONG crc, carry;
   UINT i, j;
   UCHAR uc;
   
   crc = 0xFFFFFFFF;
   for (i = 0; i < ADDR_SIZE; i++) {
      uc = pAddress[i];
      for (j = 0; j < 8; j++) {
         carry = ((crc & 0x80000000) ? 1 : 0) ^ (uc & 0x01);
         crc <<= 1;
         uc >>= 1;
         if (carry) crc = (crc ^ 0x04c11db6) | carry;
      }
   }
   return crc;
}

//------------------------------------------------------------------------------

BOOL AM79C973InitDMABuffer(DWORD address, ULONG size)
{
   ULONG offset, buffers;

   // Buffers must be aligned to 32 bytes boundary
   offset = address & 0x1F;
   if (offset != 0) {
      address = address + 0x20 - offset;
      size = size + 0x20 - offset;
   }

   // Check if buffer is big enough to accomodate all
   buffers = TX_BUFFERS + RX_BUFFERS;
   if (size < ((BUFFER_SIZE + DESC_SIZE) * buffers + INIT_SIZE)) return FALSE;

   // Store address and size
   g_dmaAddress = C2U(address);
   g_dmaSize = size;
   
   return TRUE;
}

//------------------------------------------------------------------------------

BOOL AM79C973Init(UCHAR *pAddress, ULONG offset, USHORT mac[3])
{
   ULONG i, pos;
   USHORT data;
   
   g_base = C2U(pAddress);

   // Switch to DWIO mode
   OUT32(g_base + RDP, 0);

   // Set software style to 3 (32bit software structure)
   WriteBCR(20, 0x0503);

   // Stop adapter
   WriteCSR(0, 0x0004);

   // Divide DMA buffer
   pos = g_dmaAddress;
   g_pInit = (ULONG*)pos;
   pos += INIT_SIZE;
   g_pRxRing = (ULONG*)pos;
   pos += RX_BUFFERS * DESC_SIZE;
   g_pTxRing = (ULONG*)pos;
   pos += TX_BUFFERS * DESC_SIZE;
   g_pRxBuffer = (UCHAR*)pos;
   pos += RX_BUFFERS * BUFFER_SIZE;
   g_pTxBuffer = (UCHAR*)pos;

   // Prepare initialization block
   g_pInit[0] = 0x20500180;
   g_pInit[1] = INP32(g_base);
   g_pInit[2] = INP32(g_base + 4);
   g_pInit[3] = 0;
   g_pInit[4] = 0;
   g_pInit[5] = V2P(g_pRxRing);
   g_pInit[6] = V2P(g_pTxRing);

   // Save MAC address
   memcpy(mac, &g_pInit[1], 6);

   // Initialize RX ring   
   for (i = 0; i < RX_BUFFERS; i++) {
      g_pRxRing[4 * i + 0] = 0;
      g_pRxRing[4 * i + 1] = RMD1_OWN | RMD1_ONES | (4096-BUFFER_SIZE);
      g_pRxRing[4 * i + 2] = V2P(g_pRxBuffer + i * BUFFER_SIZE);
      g_pRxRing[4 * i + 3] = (ULONG)(g_pRxBuffer + i * BUFFER_SIZE);
   }
   g_rxPos = 0;
   
   // Initialize TX ring   
   for (i = 0; i < TX_BUFFERS; i++) {
      g_pTxRing[4 * i + 0] = 0;
      g_pTxRing[4 * i + 1] = 0;
      g_pTxRing[4 * i + 2] = V2P(g_pTxBuffer + i * BUFFER_SIZE);
      g_pTxRing[4 * i + 3] = (ULONG)(g_pTxBuffer + i * BUFFER_SIZE);
   }
   g_txPos = 0;
   
   // Set initialization block address
   pos = V2P(g_pInit);
   WriteCSR(1, pos & 0xFFFF);
   WriteCSR(2, pos >> 16);

   // Connect internal PHY
   Log("Am79C973: Autonegotiation start...");
   WriteMII(0, 0x3100);
   OEMSleep(10);

   // Wait autonegotiation complete or link is up.
   while ((ReadMII(1) & 0x0020) == 0) OEMSleep(10);
   Log("  and completed, link mode ");

   data = ReadMII(24);
   if ((data & 0x0001) != 0) Log("100BASE-TX "); else Log("10BASE-T ");
   if ((data & 0x0004) != 0) Log("FD\n"); else Log("HD\n");

   // Mask everything
   WriteCSR(3, 0x1F40); // Enable DXSUFLO to let it recover from underflow
   WriteCSR(4, 0x0914);

   // Start initialization
   WriteCSR(0, 0x0001);

   // Wait for initialization complete
   while ((ReadCSR(0) & 0x0100) == 0) OEMSleep(10);

   // Enable Tx/Rx
   WriteCSR(0, 0x0002);

   return TRUE;
}

//------------------------------------------------------------------------------

USHORT AM79C973SendFrame(UCHAR *pData, ULONG length)
{
   ULONG start;
   volatile ULONG *pos;

   // Wait until buffer is done
   start = OEMEthGetSecs();
   pos = (volatile ULONG*)&g_pTxRing[g_txPos << 2];

   // Wait for transmit buffer available
   while ((pos[1] & TMD1_OWN) != 0) {
      OEMSleep(1);
      if ((OEMEthGetSecs() - start) > TIMEOUT) return 1;
   }

   // Copy data to buffer
   memcpy((VOID*)pos[3], pData, length);
   pos[0] = 0;
   pos[1] = TMD1_OWN|TMD1_STP|TMD1_ENP|TMD1_ONES|(4096 - length);

   // Force controller to read tx descriptor
   WriteCSR(0, (ReadCSR(0) & 0x0040) | 0x0008);

   // Wait for DMA to complete the transfer, otherwise we hang the bus
   while ((pos[1] & TMD1_OWN) != 0) OEMSleep(1);

   // Move to next possition
   if (++g_txPos == TX_BUFFERS) g_txPos = 0;

   return 0;
}

//------------------------------------------------------------------------------

USHORT AM79C973GetFrame(UCHAR *pData, USHORT *pLength)
{
   ULONG rmd1, rmd2, length;
   volatile ULONG *pos;

   pos = (volatile ULONG*)&g_pRxRing[g_rxPos << 2];
   length = 0;

   // When packet is in buffer hardware doesn own descriptor
   while (((rmd1 = pos[1]) & RMD1_OWN) == 0) {
      rmd2 = pos[0];
      // Is packet received ok?
      length = rmd2 & 0x0FFF;
      if (length > 4) length -= 4; 
      if ((rmd1 & RMD1_ERR) == 0 && length < *pLength) {
         // Copy packet if there is no problem
         memcpy(pData, (VOID*)pos[3], length);
      } else {
         Log("AM79C973GetFrame - %X/%X %d\n", rmd1, rmd2, *pLength);
         length = 0;
      }
      // Reinitialize descriptor
      pos[0] = 0;
      pos[1] = RMD1_OWN | RMD1_ONES | (4096 - BUFFER_SIZE);
      // Move to next possition
      if (++g_rxPos == RX_BUFFERS) g_rxPos = 0;
      // Calculate position
      pos = (volatile ULONG*)&g_pRxRing[g_rxPos << 2];
      // If this descriptor is owned by hardware clear interrupt
      if ((pos[1] & RMD1_OWN) != 0) {
         WriteCSR (0, (ReadCSR(0) & 0x40) | 0x0400);
      }         
      // If we get a packet break loop
      if (length > 0) break;
   }

   // Return size
   *pLength = (USHORT)length;
   return *pLength;
}

//------------------------------------------------------------------------------

void AM79C973EnableInts()
{
   WriteCSR (3, ReadCSR(3) & 0xFBFF);  // clear RINT mask
   WriteCSR (0, 0x40);
}

//------------------------------------------------------------------------------

void AM79C973DisableInts()
{
   WriteCSR(0, 0);
}

//------------------------------------------------------------------------------

void AM79C973CurrentPacketFilter(ULONG filter)
{
   ULONG exCtrl;

   // First we must go to suspend mode
   exCtrl = ReadCSR(5);
   exCtrl |= 0x0001;
   WriteCSR(5, exCtrl);
   // Wait until we get there
   while ((ReadCSR(5) & 0x0001) == 0) OEMSleep(1);

   // Just assume that we allways receive direct & broadcast packets
   if ((filter & PACKET_TYPE_ALL_MULTICAST) != 0) {
      WriteCSR (8, 0xFFFF);
      WriteCSR (9, 0xFFFF);
      WriteCSR (10, 0xFFFF);
      WriteCSR (11, 0xFFFF);
   } else if ((filter & PACKET_TYPE_MULTICAST) == 0) {
      WriteCSR (8, 0);
      WriteCSR (9, 0);
      WriteCSR (10, 0);
      WriteCSR (11, 0);
   }

   // It is time to leave suspend mode
   exCtrl = ReadCSR(5);
   exCtrl &= ~0x0001;
   WriteCSR(5, exCtrl);

   // Wait until we get there
   while ((ReadCSR(5) & 0x0001) != 0) OEMSleep(1);
}

//------------------------------------------------------------------------------

BOOL AM79C973MulticastList(UCHAR *pAddresses, ULONG count)
{
   ULONG exCtrl, crc;
   ULONG i, j, bit;
   USHORT h[4];

   // Calculate hash bits       
   h[0] = h[1] = h[2] = h[3] = 0;
   for (i = 0; i < count; i++) {
      crc = HashAddress(pAddresses);
      bit = 0;
      for (j = 0; j < 6; j++) bit = (bit << 1) + ((crc >> j) & 0x01);
      h[bit >> 4] |= 1 << (bit & 0x0F);
      pAddresses += ADDR_SIZE;
   }

   // Go to suspend mode
   exCtrl = ReadCSR(5);
   exCtrl |= 0x0001;
   WriteCSR(5, exCtrl);

   // Wait until we get there
   while ((ReadCSR(5) & 0x0001) == 0) OEMSleep(1);

   // And set hardware   
   WriteCSR(8, h[0]);
   WriteCSR(9, h[1]);
   WriteCSR(10, h[2]);
   WriteCSR(11, h[3]);

   // Leave suspend mode
   exCtrl = ReadCSR(5);
   exCtrl &= ~0x0001;
   WriteCSR(5, exCtrl);
   
   // Wait until we get there
   while ((ReadCSR(5) & 0x0001) != 0) OEMSleep(1);

   return TRUE;
}

//------------------------------------------------------------------------------

VOID   AM79C973PowerOff()
{
}
VOID   AM79C973PowerOn()
{
}
