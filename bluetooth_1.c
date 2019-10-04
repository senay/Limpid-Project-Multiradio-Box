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

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
   int i;
   for(i=0; i<argc; i++){
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

static void print_result(bdaddr_t *bdaddr, char has_rssi, int rssi, char name[248])
{
	char addr[18];
	
	ba2str(bdaddr, addr);

	printf("%17s", addr );
	printf(" %s", name);
	if(has_rssi)
		printf(" RSSI:%d", rssi);
	else
		printf(" RSSI:n/a");
	 
	printf("\n");
	fflush(NULL);
}

int found(char *address, char traces[20][18]){
	int flag = 0, i = 0;
	
	for (i = 0; i < 20 ; i++){
		if(strcmp(address,traces[i]) == 0)
			flag = 1;
	}

	
	return flag;
}


static void scanner_start(char *filename)
{
while(1)
{
	
	int dev_id, sock = 0, signal, rssi;

	char name[248] = { 0 };
	char address[18];
	char *address_db;
	int  rssi_db;
	FILE *fp;
	FILE *pf;
	int pollret;int errno;

	struct hci_filter flt;
	//inquiry_cp cp;
	unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
	hci_event_hdr *hdr;
	char canceled = 0;
	inquiry_info_with_rssi *info_rssi;
	inquiry_info *info;
	int results, i, len;
	struct pollfd p;

	sqlite3 *db;
	char *zErrMsg = 0;
	int  rc;
	char sql[1000];
	

	dev_id = hci_get_route(NULL);
	sock = hci_open_dev( dev_id );
	if (dev_id < 0 || sock < 0) {
		perror("Can't open socket");
		return;
	}

	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_set_event(EVT_INQUIRY_RESULT, &flt);
	hci_filter_set_event(EVT_INQUIRY_RESULT_WITH_RSSI, &flt);
	hci_filter_set_event(EVT_INQUIRY_COMPLETE, &flt);
	if (setsockopt(sock, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		perror("Can't set HCI filter");
		return;
	}

	int err;
	uint8_t lap[3] = { 0x33, 0x8b, 0x9e };
	periodic_inquiry_cp cp;
	memset(&cp, 0, sizeof(cp));
	memcpy(&cp.lap, lap, 3);
	cp.max_period = 0x07;
	cp.min_period = 0x06;
	cp.length  = 0x05;
	cp.num_rsp = 0x00;
	err = hci_send_cmd(sock, OGF_LINK_CTL, OCF_PERIODIC_INQUIRY,
	        PERIODIC_INQUIRY_CP_SIZE, &cp);

	sqlite3_stmt *statement;
	printf("Starting inquiry with RSSI............................................................................\n");
	char devices_in_cur_scan[20][18] = {};
	int cnt_dev_in_cur_scan = 0;
	int alert_sent = 0;
	
	                                                                                
	rc = sqlite3_open("test.db", &db);
	sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL); 	
	if( rc ){
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		exit(0);
	}else{
		//fprintf(stdout, "Opened database successfully\n");
	}
	sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL); 


	if (hci_send_cmd (sock, OGF_LINK_CTL, OCF_INQUIRY, INQUIRY_CP_SIZE, &cp) < 0) {
		perror("Can't start inquiry");
		return;
	}

	p.fd = sock;
	p.events = POLLIN | POLLOUT | POLLPRI;

	while(!canceled) {

		pollret = poll(&p, 1, -1);
		if ( pollret> 0) {
			memset(buf,0,sizeof(buf));
			len = read(sock, buf, sizeof(buf));
			if (len < 0)
				continue;
			else if (len == 0)
				break;

			hdr = (void *) (buf + 1);
			ptr = buf + (1 + HCI_EVENT_HDR_SIZE);

			results = ptr[0];
			time_t ltime; 
			pf = fopen( filename, "a" );
			int flag = 0;
			//printf("flag0 %d\n", flag);
			//printf("new device\n");

			switch (hdr->evt) {
				case EVT_INQUIRY_RESULT:
					info = (void *)ptr + 1;
					if (hci_read_remote_name(sock, &(info_rssi)->bdaddr, sizeof(name),
					  name, 0) < 0)
					strcpy(name, "[unknown]");
					print_result(&info->bdaddr, 0, 0, name);
					break;
				case EVT_INQUIRY_RESULT_WITH_RSSI:
					//flag = 0;
					fp = fopen( "rssi.txt", "a" );
					info_rssi = (void *)ptr + 1;							
					rssi = info_rssi->rssi;
					if((rssi>-75) && (alert_sent == 0))
					{								
						//printf("Alert\n");
						ltime=time(NULL);
						//printf("Alert\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);
						fprintf(pf,"Alert\t %s\n",asctime( localtime(&ltime) ));
						alert_sent = 1;
					}		
					
					ba2str(&info_rssi->bdaddr,address);
					flag = found(address, devices_in_cur_scan);
					if(!flag){
						strcpy(devices_in_cur_scan[cnt_dev_in_cur_scan], address);
						cnt_dev_in_cur_scan++;
					}
					//printf("flag %d\n", flag);
					
					sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL); 

					
					int deviceCount = 0;
					/*Check if the device already exists in the data base*/
					sprintf(sql, "select count(address) from BLUETOOTH WHERE time > Datetime('now','-1 minute') and ADDRESS LIKE '%s';", address);
					if (sqlite3_prepare(db, sql, -1, &statement, 0 ) == SQLITE_OK ) {
						int ctotal = sqlite3_column_count(statement);
						while(1){	
							int res = sqlite3_step(statement);
							if(res == SQLITE_ROW){
								deviceCount = sqlite3_column_int(statement, 0);
								printf("deviceCount %d\n", deviceCount);
							}
										
							if ( res == SQLITE_DONE || res==SQLITE_ERROR){
								break;
							}
						}					
					}
					sqlite3_finalize(statement);
					if(deviceCount == 0){
						if (hci_read_remote_name(sock, &(info_rssi)->bdaddr, sizeof(name),
				 		 name, 0) < 0)
							strcpy(name,"[unknown]");
						//printf("name1 %s\n", name);
						print_result(&info_rssi->bdaddr, 1, info_rssi->rssi, name);
						fprintf(fp,"%d\t %s\t %17s\n",info_rssi->rssi, name, address); 
						sprintf(sql, "insert into BLUETOOTH (NAME,ADDRESS,RSSI) values ('%s','%s', %d);", name,address,rssi);
						rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
						if( rc != SQLITE_OK ){
							fprintf(stderr, "SQL error1: %s\n", zErrMsg);
							sqlite3_free(zErrMsg);
						}else{
							//fprintf(stdout, "Database updated successfully\n");
						}			
					}
					double rssi_avg = 0;
					int traces[20] = { 0 };
					int cnt_ocu = 0;
					
					if(!flag && (deviceCount != 0)){
						sprintf(sql, "select * from BLUETOOTH WHERE time > Datetime('now','-1 minute') and ADDRESS LIKE '%s'order by time ASC;", address);
						rssi_db = 0; 
						if ( sqlite3_prepare(db, sql, -1, &statement, 0 ) == SQLITE_OK ) {
							while(1){
								int res = sqlite3_step(statement);
								//printf("yes0\n");
								if(res == SQLITE_ROW){
									//printf("yes\n");
									rssi_db = sqlite3_column_int(statement, 4);
									strcpy(name , sqlite3_column_text(statement, 2));
									//printf("name2 %s\n",name);
									traces[cnt_ocu] = rssi_db;
								}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
								if(cnt_ocu == 0){
									rssi_avg = rssi_db;		
								}
								if(cnt_ocu <= 4 && cnt_ocu >0 ){

									rssi_avg = (rssi_avg*(cnt_ocu+1) + rssi_db)/(cnt_ocu + 2);
								}

								if(cnt_ocu > 4){
									rssi_avg = (rssi_avg*(cnt_ocu + 1) + rssi_db)/(cnt_ocu + 2);

									if((alert_sent == 0)&&(rssi_avg >= -87)&&traces[cnt_ocu] - traces[cnt_ocu - 1] > 0)										{	
										ltime=time(NULL); 
										printf("ALERT\n");
										alert_sent = 1;
										//printf("ALERT\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);  
										fprintf(pf,"ALERT\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);								
									}													
									if((alert_sent == 0)&&(rssi_avg > -87)&&(traces[cnt_ocu] - traces[cnt_ocu-1] > 5))										{
										ltime=time(NULL); 
										//printf("ALERT\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);  
										fprintf(pf,"ALERT\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);								alert_sent = 1;			
									}
								}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


								if ( res == SQLITE_DONE || res==SQLITE_ERROR){
									break;
								}
								fprintf(fp,"%d\t %s\t %17s\n",info_rssi->rssi, name, address);
								cnt_ocu++;
							}				
						}
						sqlite3_finalize(statement);
						print_result(&info_rssi->bdaddr, 1, info_rssi->rssi, name);
						fprintf(fp,"%d\t %s\t %17s\n",info_rssi->rssi, name, address); 
						sprintf(sql, "insert into BLUETOOTH (NAME,ADDRESS,RSSI) values ('%s','%s', %d);", name,address,rssi);
						rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
						if( rc != SQLITE_OK ){
							fprintf(stderr, "SQL error2: %s\n", zErrMsg);
							sqlite3_free(zErrMsg);
						}else{
							fprintf(stdout, "Database updated successfully\n");
						}			
					}
										
					sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
					fclose(fp);
					break;
				case EVT_INQUIRY_COMPLETE:
					canceled = 1;
					break;
			}
			fclose(pf);
		
		}
	}
	sqlite3_close(db);
	close(sock);
}	
}

int density(){
	while(1)
	{
		FILE *fp;	
		sqlite3 *db;
		int  rc;
		char *sql,density;
		char *zErrMsg = 0;
		time_t ltime; 
		rc = sqlite3_open("test.db", &db);
		if( rc ){
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
			exit(0);
		}else{
			//fprintf(stdout, "Opened database successfully\n");
		}
		fp = fopen ("spatial_density.txt", "a");
		rc=sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
		sql = "select count(address) from BLUETOOTH where time > Datetime('now','-1 minute') and rssi > -75";
		sqlite3_stmt *statement;
		if ( sqlite3_prepare(db, sql, -1, &statement, 0 ) == SQLITE_OK ) {
			int ctotal = sqlite3_column_count(statement);
			 								
			while(ctotal>=0){							
				int res = sqlite3_step(statement);
				if(res == SQLITE_ROW){
					density = sqlite3_column_int(statement, 0);
					//printf("density %d\n",density);
					ltime=time(NULL);
					//printf("%s",asctime( localtime(&ltime) ) ); 
					fprintf(fp,"%d\t %s\n",density, asctime( localtime(&ltime) ));
				}
				ctotal--;
				
				if ( res == SQLITE_DONE || res==SQLITE_ERROR){
					break;
				}
			}
		}
		fclose(fp);
		sql = "DELETE from BLUETOOTH WHERE time < Datetime('now','-45 second')";
		rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
		if( rc != SQLITE_OK ){
		fprintf(stderr, "SQL error3: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		}else{
		fprintf(stdout, "Table Cleared successfully!\n");
		}
		rc=sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
		sleep(60);
	}
	
	

}

int main(int argc, char **argv)
{
   pid_t child1, child2;
    if (!(child1 = fork())) {
        scanner_start(argv[1]);
        exit(0);
    } else if (!(child2 = fork())) {
        density();
    } else {
        wait(&child1);
            printf("got exit status from child 1\n");
        wait(&child2);
            printf("got exit status from child 2\n");
    }

    return 0;
}
