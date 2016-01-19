/*
 * Copyright (c) 2015 Opsero Electronic Design Inc.  All rights reserved.
 *
 */

/*
 * test_app.c: Test application for Ethernet FMC
 *
 * This application sets up the AXI Ethernet cores and the
 * Marvell PHYs on the Ethernet FMC to autonegotiate the
 * link speed and disable MAC address filtering.
 *
 * The main loop does the following:
 *  - force a bit error into a transmitted frame on all ports
 *  - poll the rejected frame interrupt flag for about 1 second
 *  - increment counters when dropped frames are detected
 *  - display the value of the counters
 *
 * The console will display the dropped frame counts for all
 * ports about once a second. The values should be incrementing
 * by one for each reading. In normal operation, the value of
 * the counter for each port should be the same which indicates
 * that there have been no dropped packets besides those in
 * which an error was forced by the Ethernet packet generator.
 *
 * Example (normal) output:
 *
 * Dropped frames (P0,P1,P2,P3):    1     1     1     1
 * Dropped frames (P0,P1,P2,P3):    2     2     2     2
 * Dropped frames (P0,P1,P2,P3):    3     3     3     3
 * Dropped frames (P0,P1,P2,P3):    4     4     4     4
 * Dropped frames (P0,P1,P2,P3):    5     5     5     5
 * Dropped frames (P0,P1,P2,P3):    6     6     6     6
 * Dropped frames (P0,P1,P2,P3):    7     7     7     7
 *
 * Example output where a frame was lost on port 2:
 *
 * Dropped frames (P0,P1,P2,P3):    1     1     1     1
 * Dropped frames (P0,P1,P2,P3):    2     2     2     2
 * Dropped frames (P0,P1,P2,P3):    3     3     4     3
 * Dropped frames (P0,P1,P2,P3):    4     4     5     4
 * Dropped frames (P0,P1,P2,P3):    5     5     6     5
 * Dropped frames (P0,P1,P2,P3):    6     6     7     6
 * Dropped frames (P0,P1,P2,P3):    7     7     8     7
 *
 */

#include "xparameters.h"
#include "xgpiops.h"
#include <stdio.h>
#include "xil_types.h"
#include "platform.h"
#include "xscugic.h"
#include "xiicps.h"
#include "ethfmc_axie.h"
#include "i2c_fmc.h"
#include "eeprom_fmc.h"
#include "xeth_traffic_gen.h"

/*
 * The following DEFINE sets the number of words
 * to put in the payload of the Ethernet packets to send.
 * The payload is filled with random data and the first 2
 * bytes are 0x00. The actual payload size in bytes will
 * be: (PAYLOAD_WORD_SIZE * 4) + 2
 *
 * Maximum value is 374 (1496 bytes + 2 pad bytes)
 * Minimum value is 12 (48 bytes + 2 pad bytes)
 *
 */

#define PAYLOAD_WORD_SIZE  374

#define PAYLOAD_BYTE_SIZE	((PAYLOAD_WORD_SIZE*4)+2)

#define OUTPUT_PIN		10	/* Pin connected to LED/Output */

static int SetupInterrupts(XIicPs *IicPsPtr);
XScuGic InterruptController;	/* The instance of the Interrupt Controller. */

XEth_traffic_gen eth_pkt_gen_0;
XEth_traffic_gen eth_pkt_gen_1;
XEth_traffic_gen eth_pkt_gen_2;
XEth_traffic_gen eth_pkt_gen_3;

// Pointers to the Ethernet traffic generators
XEth_traffic_gen *eth_pkt_gen_0_p = &eth_pkt_gen_0;
XEth_traffic_gen *eth_pkt_gen_1_p = &eth_pkt_gen_1;
XEth_traffic_gen *eth_pkt_gen_2_p = &eth_pkt_gen_2;
XEth_traffic_gen *eth_pkt_gen_3_p = &eth_pkt_gen_3;

XGpioPs Gpio;	/* The driver instance for GPIO Device. */

int main()
{
	int Status;
	XGpioPs_Config *ConfigPtr;
	volatile u32 reg;
	volatile u32 i;
	volatile u32 dropped_frames_0;
	volatile u32 dropped_frames_1;
	volatile u32 dropped_frames_2;
	volatile u32 dropped_frames_3;

	/*
	 * Initialize the GPIO driver.
	 */
	ConfigPtr = XGpioPs_LookupConfig(XPAR_XGPIOPS_0_DEVICE_ID);
	Status = XGpioPs_CfgInitialize(&Gpio, ConfigPtr,
					ConfigPtr->BaseAddr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XGpioPs_SetDirectionPin(&Gpio, OUTPUT_PIN, 1);
	XGpioPs_SetOutputEnablePin(&Gpio, OUTPUT_PIN, 1);
	XGpioPs_WritePin(&Gpio, OUTPUT_PIN, 0x0);

	/* the mac address of the board. this should be unique per board */
	unsigned char mac_ethernet_address[] =
	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	init_platform();

  /* Initialize I2C */
  reg = I2CInitialize();
  if(reg != XST_SUCCESS){
  	xil_printf("Failed to initialize FMC I2C!\n\r");
    return XST_FAILURE;
  }
  
	// Setup the Interrupts
	reg = SetupInterrupts(&IicInstance);
	if (reg != XST_SUCCESS) {
    	xil_printf("Failed to initialize interrupts!\n\r");
		return XST_FAILURE;
	}

  /* Test the EEPROM */
  reg = EepromTest();
  if(reg != XST_SUCCESS){
  	xil_printf("EEPROM failed the read/write test!\n\r");
    return XST_FAILURE;
  }

  // Initialize Ethernet Traffic Generators
  XEth_traffic_gen_Initialize(eth_pkt_gen_0_p,XPAR_ETH_TRAFFIC_GEN_0_DEVICE_ID);
  XEth_traffic_gen_Initialize(eth_pkt_gen_1_p,XPAR_ETH_TRAFFIC_GEN_1_DEVICE_ID);
  XEth_traffic_gen_Initialize(eth_pkt_gen_2_p,XPAR_ETH_TRAFFIC_GEN_2_DEVICE_ID);
  XEth_traffic_gen_Initialize(eth_pkt_gen_3_p,XPAR_ETH_TRAFFIC_GEN_3_DEVICE_ID);
  XEth_traffic_gen_CfgInitialize(eth_pkt_gen_0_p,XEth_traffic_gen_LookupConfig(XPAR_ETH_TRAFFIC_GEN_0_DEVICE_ID));
  XEth_traffic_gen_CfgInitialize(eth_pkt_gen_1_p,XEth_traffic_gen_LookupConfig(XPAR_ETH_TRAFFIC_GEN_1_DEVICE_ID));
  XEth_traffic_gen_CfgInitialize(eth_pkt_gen_2_p,XEth_traffic_gen_LookupConfig(XPAR_ETH_TRAFFIC_GEN_2_DEVICE_ID));
  XEth_traffic_gen_CfgInitialize(eth_pkt_gen_3_p,XEth_traffic_gen_LookupConfig(XPAR_ETH_TRAFFIC_GEN_3_DEVICE_ID));

  // Set MAC addresses
  XEth_traffic_gen_Set_dst_mac_lo_V(eth_pkt_gen_0_p,0xFFFF1E00);
  XEth_traffic_gen_Set_dst_mac_hi_V(eth_pkt_gen_0_p,0xFFFF);
  XEth_traffic_gen_Set_src_mac_lo_V(eth_pkt_gen_0_p,0xA4A52737);
  XEth_traffic_gen_Set_src_mac_hi_V(eth_pkt_gen_0_p,0xFFFF);

  XEth_traffic_gen_Set_dst_mac_lo_V(eth_pkt_gen_1_p,0xFFFF1E00);
  XEth_traffic_gen_Set_dst_mac_hi_V(eth_pkt_gen_1_p,0xFFFF);
  XEth_traffic_gen_Set_src_mac_lo_V(eth_pkt_gen_1_p,0xA4A52737);
  XEth_traffic_gen_Set_src_mac_hi_V(eth_pkt_gen_1_p,0xFFFF);

  XEth_traffic_gen_Set_dst_mac_lo_V(eth_pkt_gen_2_p,0xFFFF1E00);
  XEth_traffic_gen_Set_dst_mac_hi_V(eth_pkt_gen_2_p,0xFFFF);
  XEth_traffic_gen_Set_src_mac_lo_V(eth_pkt_gen_2_p,0xA4A52737);
  XEth_traffic_gen_Set_src_mac_hi_V(eth_pkt_gen_2_p,0xFFFF);

  XEth_traffic_gen_Set_dst_mac_lo_V(eth_pkt_gen_3_p,0xFFFF1E00);
  XEth_traffic_gen_Set_dst_mac_hi_V(eth_pkt_gen_3_p,0xFFFF);
  XEth_traffic_gen_Set_src_mac_lo_V(eth_pkt_gen_3_p,0xA4A52737);
  XEth_traffic_gen_Set_src_mac_hi_V(eth_pkt_gen_3_p,0xFFFF);

  // Set packet payload length
  XEth_traffic_gen_Set_pkt_len_V(eth_pkt_gen_0_p,PAYLOAD_WORD_SIZE);
  XEth_traffic_gen_Set_pkt_len_V(eth_pkt_gen_1_p,PAYLOAD_WORD_SIZE);
  XEth_traffic_gen_Set_pkt_len_V(eth_pkt_gen_2_p,PAYLOAD_WORD_SIZE);
  XEth_traffic_gen_Set_pkt_len_V(eth_pkt_gen_3_p,PAYLOAD_WORD_SIZE);

  // Reset force error
  XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_0_p,0);
  XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_1_p,0);
  XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_2_p,0);
  XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_3_p,0);

  // Continuous operation
  XEth_traffic_gen_EnableAutoRestart(eth_pkt_gen_0_p);
  XEth_traffic_gen_EnableAutoRestart(eth_pkt_gen_1_p);
  XEth_traffic_gen_EnableAutoRestart(eth_pkt_gen_2_p);
  XEth_traffic_gen_EnableAutoRestart(eth_pkt_gen_3_p);

  // Start the Ethernet Traffic Generators
  XEth_traffic_gen_Start(eth_pkt_gen_0_p);
  XEth_traffic_gen_Start(eth_pkt_gen_1_p);
  XEth_traffic_gen_Start(eth_pkt_gen_2_p);
  XEth_traffic_gen_Start(eth_pkt_gen_3_p);

  /* Configure the AXI Ethernet MACs and the PHYs */
	xil_printf("Ethernet Port 0:\n\r");
	EthFMC_init_axiemac(XPAR_AXIETHERNET_0_BASEADDR,mac_ethernet_address);
	xil_printf("Ethernet Port 1:\n\r");
	EthFMC_init_axiemac(XPAR_AXIETHERNET_1_BASEADDR,mac_ethernet_address);
	xil_printf("Ethernet Port 2:\n\r");
	EthFMC_init_axiemac(XPAR_AXIETHERNET_2_BASEADDR,mac_ethernet_address);
	xil_printf("Ethernet Port 3:\n\r");
	EthFMC_init_axiemac(XPAR_AXIETHERNET_3_BASEADDR,mac_ethernet_address);

	// Reset the reject frame interrupt flags
	XAxiEthernet_WriteReg(XPAR_AXIETHERNET_0_BASEADDR,XAE_IS_OFFSET,XAE_INT_RXRJECT_MASK);
	XAxiEthernet_WriteReg(XPAR_AXIETHERNET_1_BASEADDR,XAE_IS_OFFSET,XAE_INT_RXRJECT_MASK);
	XAxiEthernet_WriteReg(XPAR_AXIETHERNET_2_BASEADDR,XAE_IS_OFFSET,XAE_INT_RXRJECT_MASK);
	XAxiEthernet_WriteReg(XPAR_AXIETHERNET_3_BASEADDR,XAE_IS_OFFSET,XAE_INT_RXRJECT_MASK);

	// Reset the dropped frame counters
	dropped_frames_0 = 0;
	dropped_frames_1 = 0;
	dropped_frames_2 = 0;
	dropped_frames_3 = 0;

	while (1) {
		/* Force an error to be sent by all ports
		 * --------------------------------------
		 * The following register writes will make the Ethernet
		 * traffic generator/checker force a single bit error into
		 * a single transmitted Ethernet frame. The bit error
		 * results in a rejected (dropped) frame at the receiving
		 * end (whichever port that may be and it depends on how
		 * the ports are looped back to each other).
		 * The purpose of forcing an error like this is to ensure
		 * that our method for counting dropped frames is actually
		 * working.
		 */
		// Set force error
		XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_0_p,1);
		XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_1_p,1);
		XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_2_p,1);
		XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_3_p,1);

		// Reset force error
		XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_0_p,0);
		XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_1_p,0);
		XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_2_p,0);
		XEth_traffic_gen_Set_force_error_V(eth_pkt_gen_3_p,0);

		/* Poll for dropped packets and increment counters
		 * -----------------------------------------------
		 * This loop will repeatedly poll the rejected frame
		 * interrupt flag of the AXI Ethernet IP. If the flag
		 * is asserted, the dropped frame counter for that
		 * port is incremented and the interrupt flag is
		 * cleared by writing a 1 to it.
		 */
		for(i=0; i<1000000; i++){
			// Read the interrupt status register
			reg = XAxiEthernet_ReadReg(XPAR_AXIETHERNET_0_BASEADDR,XAE_IS_OFFSET);
			if((reg & XAE_INT_RXRJECT_MASK)){
				// Reset the interrupt
				XAxiEthernet_WriteReg(XPAR_AXIETHERNET_0_BASEADDR,XAE_IS_OFFSET,XAE_INT_RXRJECT_MASK);
				// Increment the counter
				dropped_frames_0++;
			}
			// Read the interrupt status register
			reg = XAxiEthernet_ReadReg(XPAR_AXIETHERNET_1_BASEADDR,XAE_IS_OFFSET);
			if((reg & XAE_INT_RXRJECT_MASK)){
				// Reset the interrupt
				XAxiEthernet_WriteReg(XPAR_AXIETHERNET_1_BASEADDR,XAE_IS_OFFSET,XAE_INT_RXRJECT_MASK);
				// Increment the counter
				dropped_frames_1++;
			}
			// Read the interrupt status register
			reg = XAxiEthernet_ReadReg(XPAR_AXIETHERNET_2_BASEADDR,XAE_IS_OFFSET);
			if((reg & XAE_INT_RXRJECT_MASK)){
				// Reset the interrupt
				XAxiEthernet_WriteReg(XPAR_AXIETHERNET_2_BASEADDR,XAE_IS_OFFSET,XAE_INT_RXRJECT_MASK);
				// Increment the counter
				dropped_frames_2++;
			}
			// Read the interrupt status register
			reg = XAxiEthernet_ReadReg(XPAR_AXIETHERNET_3_BASEADDR,XAE_IS_OFFSET);
			if((reg & XAE_INT_RXRJECT_MASK)){
				// Reset the interrupt
				XAxiEthernet_WriteReg(XPAR_AXIETHERNET_3_BASEADDR,XAE_IS_OFFSET,XAE_INT_RXRJECT_MASK);
				// Increment the counter
				dropped_frames_3++;
			}
		}
		/* Display the dropped frame counter values
		 * ----------------------------------------
		 * Using good Ethernet cables and an environment with low EMI,
		 * there should be no bit errors affecting the links between
		 * the ports. In this case, there should not be any dropped
		 * frames apart from those that are forced each iteration of
		 * the main loop. Thus you should expect all counters to have
		 * the same value always.
		 *
		 */
		xil_printf("Dropped frames (P0,P1,P2,P3): %8d %8d %8d %8d\n\r",
					dropped_frames_0,dropped_frames_1,dropped_frames_2,dropped_frames_3);

		if((dropped_frames_0 == dropped_frames_1) &&
				(dropped_frames_1 == dropped_frames_2) &&
				(dropped_frames_2 == dropped_frames_3) &&
				(dropped_frames_0 >= 10)){
			XGpioPs_WritePin(&Gpio, OUTPUT_PIN, 0x1);
		}
	}

	return 0;
}


/******************************************************************************/
/**
*
* Setup the interrupts for IIC.
*
*******************************************************************************/
static int SetupInterrupts(XIicPs *IicPsPtr)
{
	int Status;
	XScuGic_Config *IntcConfig; /* Instance of the interrupt controller */

	Xil_ExceptionInit();

	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(&InterruptController, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
				(Xil_ExceptionHandler)XScuGic_InterruptHandler,
				&InterruptController);

	/*
	 * Connect the device driver handler that will be called when an
	 * interrupt for the device occurs, the handler defined above performs
	 * the specific interrupt processing for the device.
	 */
	Status = XScuGic_Connect(&InterruptController, IIC_INTR_ID,
			(Xil_InterruptHandler)XIicPs_MasterInterruptHandler,
			(void *)IicPsPtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	/*
	 * Enable the interrupt for the Iic device.
	 */
	XScuGic_Enable(&InterruptController, IIC_INTR_ID);

	/*
	 * Enable interrupts in the Processor.
	 */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}


