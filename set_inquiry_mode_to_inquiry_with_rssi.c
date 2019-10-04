#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sqlite3.h> 
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <time.h>

int main()
{
	int sock, err, dev_id;
	char errno;
	struct hci_filter flt;
	

	dev_id = hci_get_route(NULL);
	sock = hci_open_dev( dev_id );
	
	//read inquiry_mode
	/*struct hci_request req;
	char raw_rp[EVT_CMD_COMPLETE_SIZE + READ_INQUIRY_MODE_RP_SIZE];
	read_inquiry_mode_rp *rp;

	req.ogf = OGF_HOST_CTL;
	req.ocf = OCF_READ_INQUIRY_MODE;
	req.event = EVT_CMD_COMPLETE;
	req.cparam = 0;
	req.clen = 0;
	req.rparam = &raw_rp;
	req.rlen = READ_INQUIRY_MODE_RP_SIZE + READ_INQUIRY_MODE_RP_SIZE;

	err = hci_send_req( sock, &req, 0 );
	printf("err :%d\n",err);
	if( err ) {
		fprintf(stderr, "error while reading inquiry mode. errno %d\n",
		errno);
		perror("hci_send_req");
		return -1;
	}*/
	//set inquiry mode to inquiry with rssi
	write_inquiry_mode_cp wim_cp;
	struct hci_request req;
	char raw_rp[EVT_CMD_COMPLETE_SIZE + WRITE_INQUIRY_MODE_RP_SIZE];
	wim_cp.mode = 2;

	req.ogf = OGF_HOST_CTL;
	req.ocf = OCF_WRITE_INQUIRY_MODE;
	req.event = EVT_CMD_COMPLETE;
	req.cparam = &wim_cp;
	req.clen = WRITE_INQUIRY_MODE_CP_SIZE;
	req.rparam = &raw_rp;
	req.rlen = EVT_CMD_COMPLETE_SIZE + WRITE_INQUIRY_MODE_RP_SIZE;

	
	if (dev_id < 0 || sock < 0) {
		perror("Can't open socket");
		return;
	}

	err = hci_send_req( sock, &req, 0 );
	if( err ) { 
		fprintf(stderr, "error while setting inquiry mode. errno %d\n",
		errno);
		perror("hci_send_req"); 
		return -1;
	}

	
}
