/******************************************************************************
*
* (c) Copyright 2010-12 Xilinx, Inc. All rights reserved.
*
* This file contains confidential and proprietary information of Xilinx, Inc.
* and is protected under U.S. and international copyright and other
* intellectual property laws.
*
* DISCLAIMER
* This disclaimer is not a license and does not grant any rights to the
* materials distributed herewith. Except as otherwise provided in a valid
* license issued to you by Xilinx, and to the maximum extent permitted by
* applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL
* FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS,
* IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF
* MERCHANTABILITY, NON-INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE;
* and (2) Xilinx shall not be liable (whether in contract or tort, including
* negligence, or under any other theory of liability) for any loss or damage
* of any kind or nature related to, arising under or in connection with these
* materials, including for any direct, or any indirect, special, incidental,
* or consequential loss or damage (including loss of data, profits, goodwill,
* or any type of loss or damage suffered as a result of any action brought by
* a third party) even if such damage or loss was reasonably foreseeable or
* Xilinx had been advised of the possibility of the same.
*
* CRITICAL APPLICATIONS
* Xilinx products are not designed or intended to be fail-safe, or for use in
* any application requiring fail-safe performance, such as life-support or
* safety devices or systems, Class III medical devices, nuclear facilities,
* applications related to the deployment of airbags, or any other applications
* that could lead to death, personal injury, or severe property or
* environmental damage (individually and collectively, "Critical
* Applications"). Customer assumes the sole risk and liability of any use of
* Xilinx products in Critical Applications, subject only to applicable laws
* and regulations governing limitations on product liability.
*
* THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE
* AT ALL TIMES.
*
******************************************************************************/
/*****************************************************************************/
/**
*
* @file xqspips.c
*
* Contains implements the interface functions of the XQspiPs driver.
* See xqspips.h for a detailed description of the device and driver.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who Date     Changes
* ----- --- -------- -----------------------------------------------
* 1.00  sdm 11/25/10 First release
*
* </pre>
*
******************************************************************************/

/***************************** Include Files *********************************/

#include "xqspips.h"

/************************** Constant Definitions *****************************/


/**************************** Type Definitions *******************************/

/**
 * This typedef defines qspi flash instruction format
 */
typedef struct {
	u8 OpCode;	/**< Operational code of the instruction */
	u8 InstSize;	/**< Size of the instruction including address bytes */
	u8 TxOffset;	/**< Register address where instruction has to be
			     written */
} XQspiPsInstFormat;

/***************** Macros (Inline Functions) Definitions *********************/

#define ARRAY_SIZE(Array)		(sizeof(Array) / sizeof((Array)[0]))

/************************** Function Prototypes ******************************/

static void XQspiPs_GetWriteData(XQspiPs *InstancePtr, u32 *Data, u8 Size);
static void XQspiPs_GetReadData(XQspiPs *InstancePtr, u32 Data, u8 Size);
static void StubStatusHandler(void *CallBackRef, u32 StatusEvent,
				unsigned ByteCount);

/************************** Variable Definitions *****************************/

/*
 * List of all the QSPI instructions and its format
 */
static XQspiPsInstFormat FlashInst[] = {
	{ XQSPIPS_FLASH_OPCODE_WREN, 1, XQSPIPS_TXD_01_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_WRDS, 1, XQSPIPS_TXD_01_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_RDSR1, 2, XQSPIPS_TXD_10_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_RDSR2, 2, XQSPIPS_TXD_10_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_WRSR, 2, XQSPIPS_TXD_10_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_PP, 4, XQSPIPS_TXD_00_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_SE, 4, XQSPIPS_TXD_00_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_BE_32K, 4, XQSPIPS_TXD_00_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_BE_4K, 4, XQSPIPS_TXD_00_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_BE, 1, XQSPIPS_TXD_01_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_ERASE_SUS, 1, XQSPIPS_TXD_01_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_ERASE_RES, 1, XQSPIPS_TXD_01_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_RDID, 4, XQSPIPS_TXD_00_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_NORM_READ, 4, XQSPIPS_TXD_00_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_FAST_READ, 4, XQSPIPS_TXD_00_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_DUAL_READ, 4, XQSPIPS_TXD_00_OFFSET },
	{ XQSPIPS_FLASH_OPCODE_QUAD_READ, 4, XQSPIPS_TXD_00_OFFSET },
	/* Add all the instructions supported by the flash device */
};

/*****************************************************************************/
/**
*
* Initializes a specific XQspiPs instance such that the driver is ready to use.
*
* The state of the device after initialization is:
*   - Master mode
*   - Active high clock polarity
*   - Clock phase 0
*   - Baud rate divisor 2
*   - Transfer width 32
*   - Master reference clock = pclk
*   - No chip select active
*   - Manual CS and Manual Start disabled
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
* @param	ConfigPtr is a reference to a structure containing information
*		about a specific QSPI device. This function initializes an
*		InstancePtr object for a specific device specified by the
*		contents of Config. This function can initialize multiple
*		instance objects with the use of multiple calls giving different
*		Config information on each call.
* @param	EffectiveAddr is the device base address in the virtual memory
*		address space. The caller is responsible for keeping the address
*		mapping from EffectiveAddr to the device physical base address
*		unchanged once this function is invoked. Unexpected errors may
*		occur if the address mapping changes after this function is
*		called. If address translation is not used, use
*		ConfigPtr->Config.BaseAddress for this device.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_DEVICE_IS_STARTED if the device is already started.
*		It must be stopped to re-initialize.
*
* @note		None.
*
******************************************************************************/
int XQspiPs_CfgInitialize(XQspiPs *InstancePtr, XQspiPs_Config *ConfigPtr,
				u32 EffectiveAddr)
{
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(ConfigPtr != NULL);

	/*
	 * If the device is busy, disallow the initialize and return a status
	 * indicating it is already started. This allows the user to stop the
	 * device and re-initialize, but prevents a user from inadvertently
	 * initializing. This assumes the busy flag is cleared at startup.
	 */
	if (InstancePtr->IsBusy == TRUE) {
		return XST_DEVICE_IS_STARTED;
	}

	/*
	 * Set some default values.
	 */
	InstancePtr->IsBusy = FALSE;

	InstancePtr->Config.BaseAddress = EffectiveAddr;
	InstancePtr->StatusHandler = StubStatusHandler;

	InstancePtr->SendBufferPtr = NULL;
	InstancePtr->RecvBufferPtr = NULL;
	InstancePtr->RequestedBytes = 0;
	InstancePtr->RemainingBytes = 0;
	InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

	/*
	 * Reset the QSPI device to get it into its initial state. It is
	 * expected that device configuration will take place after this
	 * initialization is done, but before the device is started.
	 */
	XQspiPs_Reset(InstancePtr);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* Resets the QSPI device. Reset must only be called after the driver has been
* initialized. Any data transfer that is in progress is aborted.
*
* The upper layer software is responsible for re-configuring (if necessary)
* and restarting the QSPI device after the reset.
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
void XQspiPs_Reset(XQspiPs *InstancePtr)
{
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	/*
	 * Abort any transfer that is in progress
	 */
	XQspiPs_Abort(InstancePtr);

	/*
	 * Reset any values that are not reset by the hardware reset such that
	 * the software state matches the hardware device
	 */
	XQspiPs_WriteReg(InstancePtr->Config.BaseAddress, XQSPIPS_CR_OFFSET,
			  XQSPIPS_CR_RESET_STATE);
}

/*****************************************************************************/
/**
*
* Aborts a transfer in progress by disabling the device and resetting the FIFOs
* if present. The byte counts are cleared, the busy flag is cleared, and mode
* fault is cleared.
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
*
* @return	None.
*
* @note
*
* This function does a read/modify/write of the control register. The user of
* this function needs to take care of critical sections.
*
******************************************************************************/
void XQspiPs_Abort(XQspiPs *InstancePtr)
{

	XQspiPs_Disable(InstancePtr->Config.BaseAddress);

	/*
	 * Clear the RX FIFO and drop any data.
	 */
	while ((XQspiPs_ReadReg(InstancePtr->Config.BaseAddress,
		 XQSPIPS_SR_OFFSET) & XQSPIPS_IXR_RXNEMPTY_MASK) != 0) {
		XQspiPs_ReadReg(InstancePtr->Config.BaseAddress,
				 XQSPIPS_RXD_OFFSET);
	}

	/*
	 * Clear mode fault condition.
	 */
	XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
			  XQSPIPS_SR_OFFSET, XQSPIPS_IXR_MODF_MASK);

	InstancePtr->RemainingBytes = 0;
	InstancePtr->RequestedBytes = 0;
	InstancePtr->IsBusy = FALSE;
}

/*****************************************************************************/
/**
*
* Transfers specified data on the QSPI bus. If the QSPI device is configured as
* a master, this function initiates bus communication and sends/receives the
* data to/from the selected QSPI slave. If the QSPI device is configured as a
* slave, this function prepares the buffers to be sent/received when selected
* by a master. For every byte sent, a byte is received.
*
* The caller has the option of providing two different buffers for send and
* receive, or one buffer for both send and receive, or no buffer for receive.
* The receive buffer must be at least as big as the send buffer to prevent
* unwanted memory writes. This implies that the byte count passed in as an
* argument must be the smaller of the two buffers if they differ in size.
* Here are some sample usages:
* <pre>
*   XQspiPs_Transfer(InstancePtr, SendBuf, RecvBuf, ByteCount)
*	The caller wishes to send and receive, and provides two different
*	buffers for send and receive.
*
*   XQspiPs_Transfer(InstancePtr, SendBuf, NULL, ByteCount)
*	The caller wishes only to send and does not care about the received
*	data. The driver ignores the received data in this case.
*
*   XQspiPs_Transfer(InstancePtr, SendBuf, SendBuf, ByteCount)
*	The caller wishes to send and receive, but provides the same buffer
*	for doing both. The driver sends the data and overwrites the send
*	buffer with received data as it transfers the data.
*
*   XQspiPs_Transfer(InstancePtr, RecvBuf, RecvBuf, ByteCount)
*	The caller wishes to only receive and does not care about sending
*	data.  In this case, the caller must still provide a send buffer, but
*	it can be the same as the receive buffer if the caller does not care
*	what it sends.  The device must send N bytes of data if it wishes to
*	receive N bytes of data.
* </pre>
* Although this function takes entire buffers as arguments, the driver can only
* transfer a limited number of bytes at a time, limited by the size of the
* FIFO. A call to this function only starts the transfer, then subsequent
* transfers of the data is performed by the interrupt service routine until
* the entire buffer has been transferred. The status callback function is
* called when the entire buffer has been sent/received.
*
* This function is non-blocking. As a master, the SetSlaveSelect function must
* be called prior to this function.
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
* @param	SendBufPtr is a pointer to a data buffer that needs to be
*		transmitted. This buffer must not be NULL.
* @param	RecvBufPtr is a pointer to a buffer for received data.
*		This argument can be NULL if do not care about receiving.
* @param	ByteCount contains the number of bytes to send/receive.
*		The number of bytes received always equals the number of bytes
*		sent.
* @param	IsInst specifies whether the first byte(s) in the transmit
*		buffer is a serial flash instruction.
*
* @return
*		- XST_SUCCESS if the buffers are successfully handed off to the
*		  device for transfer.
*		- XST_DEVICE_BUSY indicates that a data transfer is already in
*		  progress. This is determined by the driver.
*
* @note
*
* This function is not thread-safe.  The higher layer software must ensure that
* no two threads are transferring data on the QSPI bus at the same time.
*
******************************************************************************/
int XQspiPs_Transfer(XQspiPs *InstancePtr, u8 *SendBufPtr, u8 *RecvBufPtr,
		      unsigned ByteCount, int IsInst)
{
	u32 ControlReg;
	u8 Instruction;
	u32 Data;
	unsigned int Index;
	XQspiPsInstFormat *CurrInst;

	/*
	 * The RecvBufPtr argument can be null
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(SendBufPtr != NULL);
	Xil_AssertNonvoid(ByteCount > 0);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertNonvoid(IsInst <= XQSPIPS_IS_INST);

	/*
	 * Check whether there is another transfer in progress. Not thread-safe.
	 */
	if (InstancePtr->IsBusy) {
		return XST_DEVICE_BUSY;
	}

	/*
	 * Set the busy flag, which will be cleared in the ISR when the
	 * transfer is entirely done.
	 */
	InstancePtr->IsBusy = TRUE;

	/*
	 * Set up buffer pointers.
	 */
	InstancePtr->SendBufferPtr = SendBufPtr;
	InstancePtr->RecvBufferPtr = RecvBufPtr;

	InstancePtr->RequestedBytes = ByteCount;
	InstancePtr->RemainingBytes = ByteCount;

	/*
	 * If the slave select lines are "Forced" or under manual control,
	 * set the slave selects now, before beginning the transfer.
	 */
	ControlReg = XQspiPs_ReadReg(InstancePtr->Config.BaseAddress,
				      XQSPIPS_CR_OFFSET);
	if (0 != (ControlReg & XQSPIPS_CR_SSFORCE_MASK)) {

		ControlReg &= ~XQSPIPS_CR_SSCTRL_MASK;
		ControlReg |= InstancePtr->SlaveSelect;
		XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
				  XQSPIPS_CR_OFFSET, ControlReg);
	}

	/*
	 * Enable the device.
	 */
	XQspiPs_Enable(InstancePtr->Config.BaseAddress);

	if (IsInst == 1) {
		Instruction = *InstancePtr->SendBufferPtr;

		for (Index = 0 ; Index < ARRAY_SIZE(FlashInst); Index++) {
			if (Instruction == FlashInst[Index].OpCode) {
				break;
			}
		}

		if (Index == ARRAY_SIZE(FlashInst)) {
			/* Instruction is not supported */
			return XST_FAILURE;
		}

		CurrInst = &FlashInst[Index];

		/* Get the complete command (flash inst + address/data) */
		Data = 0;
		XQspiPs_GetWriteData(InstancePtr, &Data,
				      CurrInst->InstSize);

		/* Write the command to the FIFO */
		XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
				  CurrInst->TxOffset, Data);
	}

	/*
	 * Fill the Tx FIFO with as many bytes as it takes (or as many as
	 * we have to send).
	 */
	while ((InstancePtr->RemainingBytes > 0) &&
	       ((XQspiPs_ReadReg(InstancePtr->Config.BaseAddress,
				  XQSPIPS_SR_OFFSET) &
				  XQSPIPS_IXR_TXFULL_MASK) == 0)) {

		if (InstancePtr->RemainingBytes < 4) {
			XQspiPs_GetWriteData(InstancePtr, &Data,
					      InstancePtr->RemainingBytes);
		} else {
			XQspiPs_GetWriteData(InstancePtr, &Data, 4);
		}

		XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
				  XQSPIPS_TXD_00_OFFSET, Data);
	}

	/*
	 * Enable QSPI interrupts (connecting to the interrupt controller and
	 * enabling interrupts should have been done by the caller).
	 */
	XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
			  XQSPIPS_IER_OFFSET, XQSPIPS_IXR_DFLT_MASK);

	/*
	 * If, in Manual Start mode, Start the transfer.
	 */
	if ((ControlReg & (XQSPIPS_CR_MSTREN_MASK |
			   XQSPIPS_CR_MANSTRTEN_MASK)) ==
	    (XQSPIPS_CR_MSTREN_MASK | XQSPIPS_CR_MANSTRTEN_MASK)) {

		ControlReg |= XQSPIPS_CR_MANSTRT_MASK;
		XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
				  XQSPIPS_CR_OFFSET, ControlReg);
	}

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
* Transfers specified data on the QSPI bus in polled mode.
*
* The caller has the option of providing two different buffers for send and
* receive, or one buffer for both send and receive, or no buffer for receive.
* The receive buffer must be at least as big as the send buffer to prevent
* unwanted memory writes. This implies that the byte count passed in as an
* argument must be the smaller of the two buffers if they differ in size.
* Here are some sample usages:
* <pre>
*   XQspiPs_PolledTransfer(InstancePtr, SendBuf, RecvBuf, ByteCount)
*	The caller wishes to send and receive, and provides two different
*	buffers for send and receive.
*
*   XQspiPs_PolledTransfer(InstancePtr, SendBuf, NULL, ByteCount)
*	The caller wishes only to send and does not care about the received
*	data. The driver ignores the received data in this case.
*
*   XQspiPs_PolledTransfer(InstancePtr, SendBuf, SendBuf, ByteCount)
*	The caller wishes to send and receive, but provides the same buffer
*	for doing both. The driver sends the data and overwrites the send
*	buffer with received data as it transfers the data.
*
*   XQspiPs_PolledTransfer(InstancePtr, RecvBuf, RecvBuf, ByteCount)
*	The caller wishes to only receive and does not care about sending
*	data.  In this case, the caller must still provide a send buffer, but
*	it can be the same as the receive buffer if the caller does not care
*	what it sends.  The device must send N bytes of data if it wishes to
*	receive N bytes of data.
*
* </pre>
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
* @param	SendBufPtr is a pointer to a data buffer that needs to be
*		transmitted. This buffer must not be NULL.
* @param	RecvBufPtr is a pointer to a buffer for received data.
*		This argument can be NULL if do not care about receiving.
* @param	ByteCount contains the number of bytes to send/receive.
*		The number of bytes received always equals the number of bytes
*		sent.
* @param	IsInst specifies whether the first byte(s) in the transmit
*		buffer is a serial flash instruction.

* @return
*		- XST_SUCCESS if the buffers are successfully handed off to the
*		  device for transfer.
*		- XST_DEVICE_BUSY indicates that a data transfer is already in
*		  progress. This is determined by the driver.
*
* @note
*
* This function is not thread-safe.  The higher layer software must ensure that
* no two threads are transferring data on the QSPI bus at the same time.
*
******************************************************************************/
int XQspiPs_PolledTransfer(XQspiPs *InstancePtr, u8 *SendBufPtr,
			    u8 *RecvBufPtr, unsigned ByteCount, int IsInst)
{
	u32 StatusReg;
	u32 ControlReg;
	u8 Instruction;
	u32 Data;
	unsigned int Index;
	XQspiPsInstFormat *CurrInst;

	/*
	 * The RecvBufPtr argument can be NULL.
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(SendBufPtr != NULL);
	Xil_AssertNonvoid(ByteCount > 0);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertNonvoid(IsInst <= XQSPIPS_IS_INST);

	/*
	 * Check whether there is another transfer in progress. Not thread-safe.
	 */
	if (InstancePtr->IsBusy) {
		return XST_DEVICE_BUSY;
	}

	/*
	 * Set the busy flag, which will be cleared when the transfer is
	 * entirely done.
	 */
	InstancePtr->IsBusy = TRUE;

	/*
	 * Set up buffer pointers.
	 */
	InstancePtr->SendBufferPtr = SendBufPtr;
	InstancePtr->RecvBufferPtr = RecvBufPtr;

	InstancePtr->RequestedBytes = ByteCount;
	InstancePtr->RemainingBytes = ByteCount;

	/*
	 * If the slave select lines are "Forced" or under manual control,
	 * set the slave selects now, before beginning the transfer.
	 */
	ControlReg = XQspiPs_ReadReg(InstancePtr->Config.BaseAddress,
				      XQSPIPS_CR_OFFSET);
	if (0 != (ControlReg & XQSPIPS_CR_SSFORCE_MASK)){

		ControlReg &= ~XQSPIPS_CR_SSCTRL_MASK;
		ControlReg |= InstancePtr->SlaveSelect;

		XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
				  XQSPIPS_CR_OFFSET, ControlReg);
	}

	/*
	 * Enable the device.
	 */
	XQspiPs_Enable(InstancePtr->Config.BaseAddress);

	if (IsInst == 1) {
		Instruction = *InstancePtr->SendBufferPtr;

		for (Index = 0 ; Index < ARRAY_SIZE(FlashInst); Index++) {
			if (Instruction == FlashInst[Index].OpCode) {
				break;
			}
		}

		if (Index == ARRAY_SIZE(FlashInst)) {
			/* Instruction not supported */
			return XST_FAILURE;
		}

		CurrInst = &FlashInst[Index];

		/* Get the complete command (flash inst + address/data) */
		Data = 0;
		XQspiPs_GetWriteData(InstancePtr, &Data,
				      CurrInst->InstSize);

		/* Write the command to the FIFO */
		XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
				  CurrInst->TxOffset, Data);
	}

	while((InstancePtr->RemainingBytes > 0) ||
	      (InstancePtr->RequestedBytes > 0)) {

		/*
		 * Fill the DTR/FIFO with as many bytes as it will take (or as
		 * many as we have to send).
		 */
		while ((InstancePtr->RemainingBytes > 0) &&
		       ((XQspiPs_ReadReg(InstancePtr->Config.BaseAddress,
					  XQSPIPS_SR_OFFSET) &
					  XQSPIPS_IXR_TXFULL_MASK) == 0)) {

			if (InstancePtr->RemainingBytes < 4) {
				XQspiPs_GetWriteData(InstancePtr, &Data,
						InstancePtr->RemainingBytes);
			} else {
				XQspiPs_GetWriteData(InstancePtr, &Data, 4);
			}

			XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
					  XQSPIPS_TXD_00_OFFSET, Data);
		}

		/*
		 * If, in Manual Start mode, start the transfer.
		 */
		if ((ControlReg & (XQSPIPS_CR_MSTREN_MASK |
				   XQSPIPS_CR_MANSTRTEN_MASK)) ==
		    (XQSPIPS_CR_MSTREN_MASK | XQSPIPS_CR_MANSTRTEN_MASK)) {
			ControlReg |= XQSPIPS_CR_MANSTRT_MASK;
			XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
					  XQSPIPS_CR_OFFSET, ControlReg);
		}

		/*
		 * Wait for the transfer to finish by polling Tx fifo status.
		 */
		do {
			StatusReg = XQspiPs_ReadReg(
					InstancePtr->Config.BaseAddress,
					XQSPIPS_SR_OFFSET);
		} while ((StatusReg & 0x04) == 0);

		/*
		 * A transmit has just completed. Process received data
		 * and check for more data to transmit.
		 * First get the data received as a result of the
		 * transmit that just completed. We get all the data
		 * available by reading the status register to determine
		 * when the Receive register/FIFO is empty. Always get
		 * the received data, but only fill the receive
		 * buffer if it points to something (the upper layer
		 * software may not care to receive data).
		 */
		while ((StatusReg & XQSPIPS_IXR_RXNEMPTY_MASK) != 0) {
			u32 Data;

			Data = XQspiPs_ReadReg(InstancePtr->Config.BaseAddress,
						XQSPIPS_RXD_OFFSET);

			if (InstancePtr->RequestedBytes < 4) {
				XQspiPs_GetReadData(InstancePtr, Data,
						InstancePtr->RequestedBytes);
			} else {
				XQspiPs_GetReadData(InstancePtr, Data, 4);
			}
			StatusReg = XQspiPs_ReadReg(
						InstancePtr->Config.BaseAddress,
						XQSPIPS_SR_OFFSET);
		}
	}

	/*
	 * If the Slave select lines are being manually controlled, disable
	 * them because the transfer is complete.
	 */
	if (0 != (ControlReg & XQSPIPS_CR_SSFORCE_MASK)) {
		ControlReg |= XQSPIPS_CR_SSCTRL_MASK;
		XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
				  XQSPIPS_CR_OFFSET, ControlReg);
	}

	InstancePtr->IsBusy = FALSE;

	/*
	 * Disable the device.
	 */
	XQspiPs_Disable(InstancePtr->Config.BaseAddress);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* Read the flash in Linear QSPI mode.
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
* @param	RecvBufPtr is a pointer to a buffer for received data.
* @param	Address is the starting address within the flash from
*		from where data needs to be read.
* @param	ByteCount contains the number of bytes to receive.
*
* @return	None.
*
* @note
*
* This function assumes that QSPI device in Linear mode. If the device is in
* normal mode unexpected errors will occur.
*
******************************************************************************/
void XQspiPs_LqspiRead(XQspiPs *InstancePtr, u8 *RecvBufPtr,
			u32 Address, unsigned ByteCount)
{
	memcpy((void*)RecvBufPtr, (const void*)(XPAR_XQSPIPS_0_LINEAR_BASEADDR +
		Address), (size_t)ByteCount);
}

/*****************************************************************************/
/**
*
* Selects the slave with which the master communicates.
*
* The user is not allowed to select the slave while a transfer is in progress.
* If no transfer is in progress, the user can select a new slave, which
* implicitly deselects the current slave.
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
* @param	SlaveSel is the chip select line which needs to be asserted.
*		A value of 0 asserts the first chip select line. Only one slave
*		can be selected at a time.
*
* @return
*		- XST_SUCCESS if the slave is selected or deselected
*		  successfully.
*		- XST_DEVICE_BUSY if a transfer is in progress, slave cannot be
*		  changed.
*
* @note
*
* This function only sets the slave which will be selected when a transfer
* occurs. The slave is not selected when the QSPI is idle. The slave select
* has no affect when the device is configured as a slave.
*
******************************************************************************/
int XQspiPs_SetSlaveSelect(XQspiPs *InstancePtr, u8 SlaveSel)
{
	u32 ControlReg;

	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertNonvoid(SlaveSel <= XQSPIPS_CR_SSCTRL_MAXIMUM);

	/*
	 * Do not allow the slave select to change while a transfer is in
	 * progress. Not thread-safe.
	 */
	if (InstancePtr->IsBusy) {
		return XST_DEVICE_BUSY;
	}

	InstancePtr->SlaveSelect = (((~(0x0001 << SlaveSel)) <<
				     XQSPIPS_CR_SSCTRL_SHIFT) &
				    XQSPIPS_CR_SSCTRL_MASK);

	ControlReg = XQspiPs_ReadReg(InstancePtr->Config.BaseAddress,
				      XQSPIPS_CR_OFFSET);

	/*
	 * Select the slaves
	 */
	ControlReg &= ~XQSPIPS_CR_SSCTRL_MASK;
	ControlReg |= InstancePtr->SlaveSelect;

	XQspiPs_WriteReg(InstancePtr->Config.BaseAddress,
			  XQSPIPS_CR_OFFSET, ControlReg);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* Gets the current slave select setting for the QSPI device.
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
*
* @return	The value of the SPI_ss_outN bits in the control register, if
*		the slave selects are in manual control, or,
*		The value of SlaveSelect, in XQspiPs instance.
*
* @note		None.
*
******************************************************************************/
u8 XQspiPs_GetSlaveSelect(XQspiPs *InstancePtr)
{
	u32 ControlReg;

	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	ControlReg = XQspiPs_ReadReg(InstancePtr->Config.BaseAddress,
				      XQSPIPS_CR_OFFSET);

	if (0 != (ControlReg & XQSPIPS_CR_SSFORCE_MASK)) {
		ControlReg &= XQSPIPS_CR_SSCTRL_MASK;
		ControlReg >>= XQSPIPS_CR_SSCTRL_SHIFT;
	} else {
		ControlReg = InstancePtr->SlaveSelect;
	}

	return ((u8) ControlReg);
}

/*****************************************************************************/
/**
*
* Sets the status callback function, the status handler, which the driver
* calls when it encounters conditions that should be reported to upper
* layer software. The handler executes in an interrupt context, so it must
* minimize the amount of processing performed. One of the following status
* events is passed to the status handler.
*
* <pre>
* XST_SPI_MODE_FAULT		A mode fault error occurred, meaning the device
*				is selected as slave while being a master.
*
* XST_SPI_TRANSFER_DONE		The requested data transfer is done
*
* XST_SPI_TRANSMIT_UNDERRUN	As a slave device, the master clocked data
*				but there were none available in the transmit
*				register/FIFO. This typically means the slave
*				application did not issue a transfer request
*				fast enough, or the processor/driver could not
*				fill the transmit register/FIFO fast enough.
*
* XST_SPI_RECEIVE_OVERRUN	The QSPI device lost data. Data was received
*				but the receive data register/FIFO was full.
*
* XST_SPI_SLAVE_MODE_FAULT	A slave QSPI device was selected as a slave
*				while it was disabled. This indicates the
*				master is already transferring data (which is
*				being dropped until the slave application
*				issues a transfer).
* </pre>
* @param	InstancePtr is a pointer to the XQspiPs instance.
* @param	CallBackRef is the upper layer callback reference passed back
*		when the callback function is invoked.
* @param	FuncPtr is the pointer to the callback function.
*
* @return	None.
*
* @note
*
* The handler is called within interrupt context, so it should do its work
* quickly and queue potentially time-consuming work to a task-level thread.
*
******************************************************************************/
void XQspiPs_SetStatusHandler(XQspiPs *InstancePtr, void *CallBackRef,
				XQspiPs_StatusHandler FuncPtr)
{
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(FuncPtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	InstancePtr->StatusHandler = FuncPtr;
	InstancePtr->StatusRef = CallBackRef;
}

/*****************************************************************************/
/**
*
* This is a stub for the status callback. The stub is here in case the upper
* layers forget to set the handler.
*
* @param	CallBackRef is a pointer to the upper layer callback reference
* @param	StatusEvent is the event that just occurred.
* @param	ByteCount is the number of bytes transferred up until the event
*		occurred.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void StubStatusHandler(void *CallBackRef, u32 StatusEvent,
				unsigned ByteCount)
{
	(void) CallBackRef;
	(void) StatusEvent;
	(void) ByteCount;

	Xil_AssertVoidAlways();
}

/*****************************************************************************/
/**
*
* The interrupt handler for QSPI interrupts. This function must be connected
* by the user to an interrupt controller.
*
* The interrupts that are handled are:
*
* - Mode Fault Error. This interrupt is generated if this device is selected
*   as a slave when it is configured as a master. The driver aborts any data
*   transfer that is in progress by resetting FIFOs (if present) and resetting
*   its buffer pointers. The upper layer software is informed of the error.
*
* - Data Transmit Register (FIFO) Empty. This interrupt is generated when the
*   transmit register or FIFO is empty. The driver uses this interrupt during a
*   transmission to continually send/receive data until the transfer is done.
*
* - Data Transmit Register (FIFO) Underflow. This interrupt is generated when
*   the QSPI device, when configured as a slave, attempts to read an empty
*   DTR/FIFO.  An empty DTR/FIFO usually means that software is not giving the
*   device data in a timely manner. No action is taken by the driver other than
*   to inform the upper layer software of the error.
*
* - Data Receive Register (FIFO) Overflow. This interrupt is generated when the
*   QSPI device attempts to write a received byte to an already full DRR/FIFO.
*   A full DRR/FIFO usually means software is not emptying the data in a timely
*   manner.  No action is taken by the driver other than to inform the upper
*   layer software of the error.
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
*
* @return	None.
*
* @note
*
* The slave select register is being set to deselect the slave when a transfer
* is complete.  This is being done regardless of whether it is a slave or a
* master since the hardware does not drive the slave select as a slave.
*
******************************************************************************/
void XQspiPs_InterruptHandler(void *InstancePtr)
{
	XQspiPs *SpiPtr = (XQspiPs *)InstancePtr;
	u32 IntrStatus;
	u32 ControlReg;
	unsigned BytesDone; /* Number of bytes done so far. */

	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(SpiPtr->IsReady == XIL_COMPONENT_IS_READY);

	/*
	 * Immediately clear the interrupts in case the ISR causes another
	 * interrupt to be generated. If we clear at the end of the ISR,
	 * we may miss newly generated interrupts. This occurs because we
	 * transmit from within the ISR, which could potentially cause another
	 * TX_EMPTY interrupt.
	 */
	IntrStatus = XQspiPs_ReadReg(SpiPtr->Config.BaseAddress,
				      XQSPIPS_SR_OFFSET);
	XQspiPs_WriteReg(SpiPtr->Config.BaseAddress, XQSPIPS_SR_OFFSET,
			  IntrStatus);

	/*
	 * Check for mode fault error. We want to check for this error first,
	 * before checking for progress of a transfer, since this error needs
	 * to abort any operation in progress.
	 */
	if (XQSPIPS_IXR_MODF_MASK == (IntrStatus & XQSPIPS_IXR_MODF_MASK)) {
		BytesDone = SpiPtr->RequestedBytes - SpiPtr->RemainingBytes;

		/*
		 * Abort any operation currently in progress. This includes
		 * clearing the mode fault condition by reading the status
		 * register. Note that the status register should be read after
		 * the abort, since reading the status register clears the mode
		 * fault condition and would cause the device to restart any
		 * transfer that may be in progress.
		 */
		XQspiPs_Abort(SpiPtr);

		SpiPtr->StatusHandler(SpiPtr->StatusRef, XST_SPI_MODE_FAULT,
					BytesDone);

		return; /* Do not continue servicing other interrupts. */
	}

	while(TRUE){
		u32 Data;

		/*
		 * A transmit has just completed. Process received data and
		 * check for more data to transmit.
		 * First get the data received as a result of the transmit that
		 * just completed.  Always get the received data, but only fill
		 * the receive buffer if it is not null (it can be null when the
		 * device does not care to receive data).
		 */
		if (0 !=(IntrStatus & XQSPIPS_IXR_RXNEMPTY_MASK)){
			Data = XQspiPs_ReadReg(SpiPtr->Config.BaseAddress,
						XQSPIPS_RXD_OFFSET);

			if (SpiPtr->RequestedBytes < 4) {
				XQspiPs_GetReadData(SpiPtr, Data,
						     SpiPtr->RequestedBytes);
			} else {
				XQspiPs_GetReadData(SpiPtr, Data, 4);
			}
		}

		/*
		 * See if there is more data to send and RX FIFO is not full.
		 */
		if ((SpiPtr->RemainingBytes > 0) && (0 == (IntrStatus &
		    (XQSPIPS_IXR_TXFULL_MASK | XQSPIPS_IXR_RXFULL_MASK)))) {
			/*
			 * Send more data.
			 */
			if (SpiPtr->RemainingBytes < 4) {
				XQspiPs_GetWriteData(SpiPtr, &Data,
						      SpiPtr->RemainingBytes);
			} else {
				XQspiPs_GetWriteData(SpiPtr, &Data, 4);
			}

			XQspiPs_WriteReg(SpiPtr->Config.BaseAddress,
					  XQSPIPS_TXD_00_OFFSET, Data);
		}

		/*
		 * Exit the loop if all the data available at this time has been
		 * received and either there is no more data to send, or the
		 * FIFO is above the water mark.
		 */
		IntrStatus = XQspiPs_ReadReg(SpiPtr->Config.BaseAddress,
					      XQSPIPS_SR_OFFSET);

		if ((0 == (IntrStatus & XQSPIPS_IXR_RXNEMPTY_MASK)) &&
		    ((0 == (IntrStatus & XQSPIPS_IXR_TXOW_MASK)) ||
		     (0 == SpiPtr->RemainingBytes))) {
			break;
		}
	}

	ControlReg = XQspiPs_ReadReg(SpiPtr->Config.BaseAddress,
				      XQSPIPS_CR_OFFSET);
	if ((SpiPtr->RemainingBytes == 0) && (SpiPtr->RequestedBytes == 0)) {
		/*
		 * No more data to send.  Disable the interrupt and inform the
		 * upper layer software that the transfer is done. The interrupt
		 * will be re-enabled when another transfer is initiated.
		 */
		XQspiPs_WriteReg(SpiPtr->Config.BaseAddress,
				  XQSPIPS_IDR_OFFSET, XQSPIPS_IXR_DFLT_MASK);

		SpiPtr->IsBusy = FALSE;

		/*
		 * Disable the device.
		 */
		XQspiPs_Disable(SpiPtr->Config.BaseAddress);

		/*
		 * If the Slave select lines are being manually controlled,
		 * disable them because the transfer is complete.
		 */
		if (0 != (ControlReg & XQSPIPS_CR_SSFORCE_MASK)) {
			ControlReg |= XQSPIPS_CR_SSCTRL_MASK;
			XQspiPs_WriteReg(SpiPtr->Config.BaseAddress,
					  XQSPIPS_CR_OFFSET, ControlReg);
		}

		SpiPtr->StatusHandler(SpiPtr->StatusRef,
					XST_SPI_TRANSFER_DONE,
					SpiPtr->RequestedBytes);
	} else {
		/*
		 * If, in Manual Start mode, start the transfer.
		 */
		if ((ControlReg & (XQSPIPS_CR_MSTREN_MASK |
				   XQSPIPS_CR_MANSTRTEN_MASK)) ==
		    (XQSPIPS_CR_MSTREN_MASK | XQSPIPS_CR_MANSTRTEN_MASK)) {

			ControlReg |= XQSPIPS_CR_MANSTRT_MASK;
			XQspiPs_WriteReg(SpiPtr->Config.BaseAddress,
					  XQSPIPS_CR_OFFSET, ControlReg);
		}
	}

	/*
	 * Check for overflow and underflow errors.
	 */
	if (IntrStatus & XQSPIPS_IXR_RXOVR_MASK) {
		BytesDone = SpiPtr->RequestedBytes - SpiPtr->RemainingBytes;
		SpiPtr->IsBusy = FALSE;

		/*
		 * If the Slave select lines are being manually controlled,
		 * disable them because the transfer is complete.
		 */
		if (0 != (ControlReg & XQSPIPS_CR_SSFORCE_MASK)){
			ControlReg |= XQSPIPS_CR_SSCTRL_MASK;

			XQspiPs_WriteReg(SpiPtr->Config.BaseAddress,
				XQSPIPS_CR_OFFSET, ControlReg);
		}

		SpiPtr->StatusHandler(SpiPtr->StatusRef,
			XST_SPI_RECEIVE_OVERRUN, BytesDone);
	}

	if (IntrStatus & XQSPIPS_IXR_TXUF_MASK) {
		BytesDone = SpiPtr->RequestedBytes - SpiPtr->RemainingBytes;

		SpiPtr->IsBusy = FALSE;
		/*
		 * If the Slave select lines are being manually controlled,
		 * disable them because the transfer is complete.
		 */
		if (0 != (ControlReg & XQSPIPS_CR_SSFORCE_MASK)){

			ControlReg |= XQSPIPS_CR_SSCTRL_MASK;
			XQspiPs_WriteReg(SpiPtr->Config.BaseAddress,
					  XQSPIPS_CR_OFFSET, ControlReg);
		}

		SpiPtr->StatusHandler(SpiPtr->StatusRef,
				      XST_SPI_TRANSMIT_UNDERRUN, BytesDone);
	}
}

/*****************************************************************************/
/**
*
* Copies data from the Transmit buffer. Since QSPI supports only 32-bit
* transfers, this function appends 0xFF if the requested size is less than 4.
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
* @param	Data is a ouput parameter, to which the data read from the
*		Transmit buffer is to be copied.
* @param	Size is the number of bytes to be copied from the Transmit
*		buffer.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void XQspiPs_GetWriteData(XQspiPs *InstancePtr, u32 *Data, u8 Size)
{
	if (InstancePtr->SendBufferPtr) {
		switch (Size) {
		case 1:
			*Data = *((u8 *)InstancePtr->SendBufferPtr);
			InstancePtr->SendBufferPtr += 1;
			*Data |= 0xFFFFFF00;
			break;
		case 2:
			*Data = *((u16 *)InstancePtr->SendBufferPtr);
			InstancePtr->SendBufferPtr += 2;
			*Data |= 0xFFFF0000;
			break;
		case 3:
			*Data = *((u16 *)InstancePtr->SendBufferPtr);
			InstancePtr->SendBufferPtr += 2;
			*Data |= (*((u8 *)InstancePtr->SendBufferPtr) << 16);
			InstancePtr->SendBufferPtr += 1;
			*Data |= 0xFF000000;
			break;
		case 4:
			*Data = *((u32 *)InstancePtr->SendBufferPtr);
			InstancePtr->SendBufferPtr += 4;
			break;
		default:
			/* This will never execute */
			break;
		}
	} else
		*Data = 0;

	InstancePtr->RemainingBytes -= Size;
	if (InstancePtr->RemainingBytes < 0) {
		InstancePtr->RemainingBytes = 0;
	}
}

/*****************************************************************************/
/**
*
* Copies data from Data to the Receive buffer.
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
* @param	Data is the data which needs to be copied to the Rx buffer.
* @param	Size is the number of bytes to be copied to the Receive buffer.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void XQspiPs_GetReadData(XQspiPs *InstancePtr, u32 Data, u8 Size)
{
	u8 byte3;

	if (InstancePtr->RecvBufferPtr) {
		switch (Size) {
		case 1:
			*((u8 *)InstancePtr->RecvBufferPtr) = Data;
			InstancePtr->RecvBufferPtr += 1;
			break;
		case 2:
			*((u16 *)InstancePtr->RecvBufferPtr) = Data;
			InstancePtr->RecvBufferPtr += 2;
			break;
		case 3:
			*((u16 *)InstancePtr->RecvBufferPtr) = Data;
			InstancePtr->RecvBufferPtr += 2;
			byte3 = (u8)(Data >> 16);
			*((u8 *)InstancePtr->RecvBufferPtr) = byte3;
			InstancePtr->RecvBufferPtr += 1;
			break;
		case 4:
			(*(u32 *)InstancePtr->RecvBufferPtr) = Data;
			InstancePtr->RecvBufferPtr += 4;
			break;
		default:
			/* This will never execute */
			break;
		}
	}
	InstancePtr->RequestedBytes -= Size;
	if (InstancePtr->RequestedBytes < 0) {
		InstancePtr->RequestedBytes = 0;
	}
}
