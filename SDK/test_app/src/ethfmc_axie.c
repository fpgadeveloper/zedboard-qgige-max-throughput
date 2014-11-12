/*
 * Copyright (C) 2014 Opsero Electronic Design Inc.  All rights reserved.
 *
 * The following code is derived from parts of the lwIP example design that
 * is generated by the SDK (lwIP echo server). It contains the code that
 * is needed to configure the AXI Ethernet cores and the PHYs.
 *
 * The link speed is autonegotiated. The MACs are placed in promiscuous
 * mode which allows them to pass on packets even when they are addressed
 * to another MAC - this is necessary for the loopback because the lwIP
 * swaps the MAC addresses when it returns packets.
 */

#include "ethfmc_axie.h"
#include "stdlib.h"

unsigned EthFMC_get_IEEE_phy_speed(XAxiEthernet *xaxiemacp)
{
	u16 temp;
	u32 phy_addr = 0;
	u16 phy_identifier;
	u16 phy_model;
	u16 control;
	u16 status;
	u16 partner_capabilities;

	/* Get the PHY Identifier and Model number */
	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, 2, &phy_identifier);
	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, 3, &phy_model);
	phy_model = phy_model & PHY_MODEL_NUM_MASK;

	/* The PHY ID for Marvel is 0x0141 */
	if(phy_identifier != 0x0141)
		xil_printf("PHY ID is NOT Marvell: 0x%04X\n\r",phy_identifier);
	/* The PHY model for 88E1510 is 0x01D0 */
	if(phy_model != 0x01D0)
		xil_printf("PHY model is NOT 88E1510: 0x%04X\n\r",phy_model);
	xil_printf("Start PHY autonegotiation \r\n");

	XAxiEthernet_PhyWrite(xaxiemacp,phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 2);
	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_CONTROL_REG_MAC, &control);
	//control |= IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK;
	control &= ~(0x10);
	XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_CONTROL_REG_MAC, control);

	XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);

	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &control);
	control |= IEEE_ASYMMETRIC_PAUSE_MASK;
	control |= IEEE_PAUSE_MASK;
	control |= ADVERTISE_100;
	control |= ADVERTISE_10;
	XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, control);

	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
																	&control);
	control |= ADVERTISE_1000;
	XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
																	control);

	XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);
	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_COPPER_SPECIFIC_CONTROL_REG,
																&control);
	control |= (7 << 12);	/* max number of gigabit attempts */
	control |= (1 << 11);	/* enable downshift */
	XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_COPPER_SPECIFIC_CONTROL_REG,
																control);
	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_AUTONEGOTIATE_ENABLE;
	control |= IEEE_STAT_AUTONEGOTIATE_RESTART;

	XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);


	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_RESET_MASK;
	XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	while (1) {
		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
		if (control & IEEE_CTRL_RESET_MASK)
			continue;
		else
			break;
	}
	xil_printf("Waiting for PHY to complete autonegotiation.\r\n");

	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);
	while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
		sleep(1);
		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_COPPER_SPECIFIC_STATUS_REG_2,
																	&temp);
		if (temp & IEEE_AUTONEG_ERROR_MASK) {
			xil_printf("Auto negotiation error \r\n");
		}
		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_STATUS_REG_OFFSET,
																&status);
		}

	xil_printf("autonegotiation complete \r\n");

	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_SPECIFIC_STATUS_REG, &partner_capabilities);

	if ( ((partner_capabilities >> 14) & 3) == 2)/* 1000Mbps */
		return 1000;
	else if ( ((partner_capabilities >> 14) & 3) == 1)/* 100Mbps */
		return 100;
	else					/* 10Mbps */
		return 10;
}

unsigned EthFMC_Phy_Setup (XAxiEthernet *xaxiemacp)
{
	unsigned link_speed = 1000;

	if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
						XAE_PHY_TYPE_RGMII_1_3) {
		; /* Add PHY initialization code for RGMII 1.3 */
	} else if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
						XAE_PHY_TYPE_RGMII_2_0) {
		; /* Add PHY initialization code for RGMII 2.0 */
	} else if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
						XAE_PHY_TYPE_SGMII) {
#ifdef  CONFIG_LINKSPEED_AUTODETECT
		u32 phy_wr_data = IEEE_CTRL_AUTONEGOTIATE_ENABLE |
					IEEE_CTRL_LINKSPEED_1000M;
		phy_wr_data &= (~PHY_R0_ISOLATE);

		XAxiEthernet_PhyWrite(xaxiemacp,
				XPAR_AXIETHERNET_0_PHYADDR,
				IEEE_CONTROL_REG_OFFSET,
				phy_wr_data);
#endif
	} else if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
						XAE_PHY_TYPE_1000BASE_X) {
		; /* Add PHY initialization code for 1000 Base-X */
	}
/* set PHY <--> MAC data clock */
	link_speed = EthFMC_get_IEEE_phy_speed(xaxiemacp);
	xil_printf("auto-negotiated link speed: %d\r\n", link_speed);
	return link_speed;
}

XAxiEthernet_Config *EthFMC_xaxiemac_lookup_config(unsigned mac_base)
{
	extern XAxiEthernet_Config XAxiEthernet_ConfigTable[];
	XAxiEthernet_Config *CfgPtr = NULL;
	int i;

	for (i = 0; i < XPAR_XAXIETHERNET_NUM_INSTANCES; i++) {
		if (XAxiEthernet_ConfigTable[i].BaseAddress == mac_base) {
			CfgPtr = &XAxiEthernet_ConfigTable[i];
			break;
		}
	}

	return (CfgPtr);
}


int EthFMC_init_axiemac(unsigned mac_address,unsigned char *mac_eth_addr)
{
	unsigned link_speed = 1000;
	unsigned options;
	XAxiEthernet *axi_ethernet;
	XAxiEthernet_Config *mac_config;

	axi_ethernet = malloc(sizeof(XAxiEthernet));
	if (axi_ethernet == NULL) {
		xil_printf("EthFMC_low_level_init: out of memory\r\n");
		return -1;
	}

	/* obtain config of this emac */
	mac_config = EthFMC_xaxiemac_lookup_config(mac_address);

	XAxiEthernet_CfgInitialize(axi_ethernet, mac_config, mac_config->BaseAddress);

	options = XAxiEthernet_GetOptions(axi_ethernet);
	// Disable recognize flow control frames
	options &= ~XAE_FLOW_CONTROL_OPTION;
	//options |= XAE_FLOW_CONTROL_OPTION;
#ifdef USE_JUMBO_FRAMES
	options |= XAE_JUMBO_OPTION;
#endif
	options |= XAE_TRANSMITTER_ENABLE_OPTION;
	options |= XAE_RECEIVER_ENABLE_OPTION;
	// Disable FCS strip
	options &= ~XAE_FCS_STRIP_OPTION;
	// Disable length/type error checking
	options &= ~XAE_LENTYPE_ERR_OPTION;
	// Disable FCS insert (we have included it in the frame)
	options &= ~XAE_FCS_INSERT_OPTION;
	//options |= XAE_MULTICAST_OPTION;
	// Using promiscuous option to disable mac address filtering
	// and allow the loopback to function.
	options |= XAE_PROMISC_OPTION;
	XAxiEthernet_SetOptions(axi_ethernet, options);
	XAxiEthernet_ClearOptions(axi_ethernet, ~options);

	/* set mac address */
	XAxiEthernet_SetMacAddress(axi_ethernet, mac_eth_addr);
	link_speed = EthFMC_Phy_Setup(axi_ethernet);
    	XAxiEthernet_SetOperatingSpeed(axi_ethernet, link_speed);

	/* Setting the operating speed of the MAC needs a delay. */
	{
		volatile int wait;
		for (wait=0; wait < 100000; wait++);
		for (wait=0; wait < 100000; wait++);
	}

#ifdef NOTNOW
        /* in a soft temac implementation, we need to explicitly make sure that
         * the RX DCM has been locked. See xps_ll_temac manual for details.
         * This bit is guaranteed to be 1 for hard temac's
         */
        lock_message_printed = 0;
        while (!(XAxiEthernet_ReadReg(axi_ethernet->Config.BaseAddress, XAE_IS_OFFSET)
                    & XAE_INT_RXDCMLOCK_MASK)) {
                int first = 1;
                if (first) {
                        print("Waiting for RX DCM to lock..");
                        first = 0;
                        lock_message_printed = 1;
                }
        }

        if (lock_message_printed)
                print("RX DCM locked.\r\n");
#endif

	/* start the temac */
    XAxiEthernet_Start(axi_ethernet);

	/* enable MAC interrupts */
	XAxiEthernet_IntEnable(axi_ethernet, XAE_INT_RECV_ERROR_MASK);
  
  return 0;
}


