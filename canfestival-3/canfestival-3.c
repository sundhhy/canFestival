/*
This file is part of CanFestival, a library implementing CanOpen Stack.

Copyright (C): Cosateq GmbH & Co.KG
               http://www.cosateq.com/
               http://www.scale-rt.com/

See COPYING file for copyrights details.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
	example for application with CO-PCICAN card.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/neutrino.h>
#include <errno.h>
#include "qnxtest.h"
#include "config.h"
#include "canfestival.h"
#include "dcan_A8.h"


/* only for usleep() */
#include <unistd.h>
#define sleep_proc(ms) delay(ms)


unsigned long uptime_ms_proc()
{
	struct timespec now;
	clock_gettime( CLOCK_REALTIME, &now);

	return (now.tv_nsec / 1000000);


}




UNS8 GetChangeStateResults(UNS8 node_id, UNS8 expected_state, unsigned long timeout_ms)
{
	unsigned long start_time = 0;

	// reset nodes state
	qnxtest_Data.NMTable[node_id] = Unknown_state;

	// request node state
	masterRequestNodeState(&qnxtest_Data, node_id);

	start_time = uptime_ms_proc();
	while(uptime_ms_proc() - start_time < timeout_ms)
	{
		if (getNodeState(&qnxtest_Data, node_id) == expected_state)
		return 0;
		sleep_proc(1);
	}
	return 0xFF;
}

UNS8 ReadSDO(UNS8 nodeId, UNS16 index, UNS8 subIndex, UNS8 dataType, void* data, UNS32* size)
{
	UNS32 abortCode = 0;
	UNS8 res = SDO_UPLOAD_IN_PROGRESS;
	// Read SDO
	UNS8 err = readNetworkDict (&qnxtest_Data, nodeId, index, subIndex, dataType, 0);
	if (err)
		return 0xFF;
	for(;;)
	{
		res = getReadResultNetworkDict (&qnxtest_Data, nodeId, data, size, &abortCode);
		if (res != SDO_UPLOAD_IN_PROGRESS)
			break;
		sleep_proc(1);
		continue;
	}
	closeSDOtransfer(&qnxtest_Data, nodeId, SDO_CLIENT);
	if (res == SDO_FINISHED)
		return 0;
	return 0xFF;
}

int main(int argc, char *argv[])
{
	int port = 0;
	s_BOARD MasterBoard = {"0", "1M"};
	UNS8 node_id = 0;
	if(ThreadCtl(_NTO_TCTL_IO, 0) != EOK) {
		printf("You must be root.");
				// We will not return
		return EXIT_FAILURE;
	}


	/* process command line arguments */
	if (argc < 3)
	{
		printf("USAGE: qnxtest <node_id> <port> \n");
		return 1;
	}

	node_id = atoi(argv[1]);
	if (node_id < 2 || node_id > 127)
	{
		printf("ERROR: node_id shoule be >=2 and <= 127\n");
		return 1;
	}

	port = atoi(argv[1]);
	if (node_id < 0 || node_id > 1)
	{
		printf("WARING: node_id shoule be >=0 and <= 1, use default 0\n");
		port = 0;
	}

	if (!canOpen(&MasterBoard,&qnxtest_Data))
	{
		printf("ERROR: canInit port %d fail!\n", port);
		return 1;

	}

	setNodeId(&qnxtest_Data, node_id);

	/****************************** START *******************************/
	/* Put the master in operational mode */
	setState(&qnxtest_Data, Operational);

	/* Ask slave node to go in operational mode */
	masterSendNMTstateChange (&qnxtest_Data, 0, NMT_Start_Node);

	printf("\nStarting node %d (%xh) ...\n",(int)node_id,(int)node_id);

    /* wait untill mode will switch to operational state*/
	if (GetChangeStateResults(node_id, Operational, 3000) != 0xFF)
	{
		/* modify Client SDO 1 Parameter */
		UNS32 COB_ID_Client_to_Server_Transmit_SDO = 0x600 + node_id;
		UNS32 COB_ID_Server_to_Client_Receive_SDO  = 0x580 + node_id;
		UNS8 Node_ID_of_the_SDO_Server = node_id;
		UNS32 ExpectedSize = sizeof(UNS32);
		UNS32 ExpectedSizeNodeId = sizeof (UNS8);

		if (OD_SUCCESSFUL ==  writeLocalDict(&qnxtest_Data, 0x1280, 1, &COB_ID_Client_to_Server_Transmit_SDO, &ExpectedSize, RW)
		&& OD_SUCCESSFUL ==  writeLocalDict(&qnxtest_Data, 0x1280, 2, &COB_ID_Server_to_Client_Receive_SDO, &ExpectedSize, RW)
		&& OD_SUCCESSFUL ==  writeLocalDict(&qnxtest_Data, 0x1280, 3, &Node_ID_of_the_SDO_Server, &ExpectedSizeNodeId, RW))
		{
			UNS32 dev_type = 0;
			char device_name[64]="";
			char hw_ver[64]="";
			char sw_ver[64]="";
			UNS32 vendor_id = 0;
			UNS32 prod_code = 0;
			UNS32 ser_num = 0;
			UNS32 size;
			UNS8 res;

			printf("\nnode_id: %d (%xh) info\n",(int)node_id,(int)node_id);
			printf("********************************************\n");

			size = sizeof (dev_type);
			res = ReadSDO(node_id, 0x1000, 0, uint32, &dev_type, &size);
			printf("device type: %d\n",dev_type & 0xFFFF);

			size = sizeof (device_name);
			res = ReadSDO(node_id, 0x1008, 0, visible_string, device_name, &size);
			printf("device name: %s\n",device_name);

			size = sizeof (hw_ver);
			res = ReadSDO(node_id, 0x1009, 0, visible_string, hw_ver, &size);
			printf("HW version: %s\n",hw_ver);

			size = sizeof (sw_ver);
			res = ReadSDO(node_id, 0x100A, 0, visible_string, sw_ver, &size);
			printf("SW version: %s\n",sw_ver);

			size = sizeof (vendor_id);
			res = ReadSDO(node_id, 0x1018, 1, uint32, &vendor_id, &size);
			printf("vendor id: %d\n",vendor_id);

			size = sizeof (prod_code);
			res = ReadSDO(node_id, 0x1018, 2, uint32, &prod_code, &size);
			printf("product code: %d\n",prod_code);

			size = sizeof (ser_num);
			res = ReadSDO(node_id, 0x1018, 4, uint32, &ser_num, &size);
			printf("serial number: %d\n",ser_num);

			printf("********************************************\n");
		}
		else
		{
			printf("ERROR: Object dictionary access failed\n");
		}
	}
	else
	{
		printf("ERROR: node_id %d (%xh) is not responding\n",(int)node_id,(int)node_id);
	}

	masterSendNMTstateChange (&qnxtest_Data, 0x02, NMT_Stop_Node);

	setState(&qnxtest_Data, Stopped);

	canClose(&qnxtest_Data);


  return 0;
}

