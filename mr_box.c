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
int ind = 0;
static int callback(void *NotUsed, int argc, char **argv, char **azColName){
   int i;
   for(i=0; i<argc; i++){
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

time_t convert(char time_text[19]){
	struct tm tm;
	time_t t;
	char *buf;
	buf=time_text;
	strptime(buf, "%Y-%m-%d %H:%M:%S", &tm);
	tm.tm_isdst = -1;      /* Not set by strptime(); tells mktime()
		                  to determine whether daylight saving time
		                  is in effect */
	t = mktime(&tm);
	if (t == -1)
	exit(0);
	return t;
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

int found(char *address, char traces[20][18], int size){
	int flag = 0, i = 0;
	
	for (i = 0; i < size ; i++){
		if(strcmp(address,traces[i]) == 0){
			flag = 1;
			ind = i;
		}
	}	
	return flag;
}

static void Inquiry_Start(char *filename, char *rssi_file)
{
	
	sqlite3 *db;
	time_t alert_time = 0;
	int  rc;
	
	rc = sqlite3_open("bluetooth_devices.db", &db);
 	
	if( rc ){
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		exit(0);
	}else{
		//fprintf(stdout, "Opened database successfully\n");
	}
while(1)
{
	
	int dev_id, sock = 0, signal, rssi;

	char name[248] = { 0 };
	strcpy(name,"name");
	char time_db_f[248] = { 0 };
	char time_db_s[248] = { 0 };
	char address[18];
	char address_same[18];
	char *address_db;
	int  rssi_db;
	FILE *fp;
	FILE *pf;
	int pollret;int errno;

	struct hci_filter flt;
	unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
	hci_event_hdr *hdr;
	char canceled = 0;
	extended_inquiry_info *info_extended;
	inquiry_info_with_rssi *info_rssi;
	inquiry_info *info;
	int results, i, len;
	struct pollfd p;

	char *zErrMsg = 0;
	
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
	hci_filter_set_event(EVT_EXTENDED_INQUIRY_RESULT, &flt);
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
	cp.max_period = 0x0A;
	cp.min_period = 0x09;
	cp.length  = 0x08;
	cp.num_rsp = 0x00;
	err = hci_send_cmd(sock, OGF_LINK_CTL, OCF_PERIODIC_INQUIRY,
	        PERIODIC_INQUIRY_CP_SIZE, &cp);
	sqlite3_stmt *statement;
	//printf("Starting inquiry with RSSI............................................................................\n");
	
	char devices_in_cur_scan[20][18] = {};
	int cnt_dev_in_cur_scan = 0;
	int alert_sent = 0;
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
			time_t ltime,current_time; 

			pf = fopen( filename, "a" );
			int flag_current = 0;
			int deviceCount = 0;
			int derivative = 0;
			int derivative_db = 0;
			double rssi_avg = 0;
			int traces[20] = { 0 };
			int cnt_ocu = 0;
			switch (hdr->evt) {
				case EVT_INQUIRY_RESULT:
					info = (void *)ptr + 1;
					if (hci_read_remote_name(sock, &(info_rssi)->bdaddr, sizeof(name),
					  name, 0) < 0)
					strcpy(name, "[unknown]");
					print_result(&info->bdaddr, 0, 0, name);
					break;
				case EVT_EXTENDED_INQUIRY_RESULT:
					fp = fopen( rssi_file, "a" );
					info_extended = (void *)ptr + 1;
					rssi = info_extended->rssi;	
					//strncpy(name,info_extended->data,9);
					ba2str(&info_extended->bdaddr,address);
					rc=sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
					current_time = time(NULL);
					double k;
					k = difftime(current_time,0.00);
					fprintf(fp,"%d\t %17s\t %d\t %s\t\n",info_extended->rssi, address, (int) k, name);
	
					/*Check if the device already exists in the data base*/
					sprintf(sql, "select count(address) from BLUETOOTH WHERE time > Datetime('now','localtime','-1 minute') and ADDRESS LIKE '%s';", address);
					if (sqlite3_prepare(db, sql, -1, &statement, 0 ) == SQLITE_OK ) {
						int ctotal = sqlite3_column_count(statement);
						while(1){	
							int res = sqlite3_step(statement);
							if(res == SQLITE_ROW){
								deviceCount = sqlite3_column_int(statement, 0);
							}
										
							if ( res == SQLITE_DONE || res==SQLITE_ERROR){
								break;
							}
						}					
					}
					sqlite3_finalize(statement);
					rc=sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
					rc=sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
					//printf("Device Number in the database:%d\n", deviceCount);
					if(deviceCount==0){
						sprintf(sql, "insert into BLUETOOTH (NAME,ADDRESS,RSSI,DERIVATIVE) values ('%s','%s', %d,%d);", name,address,rssi,derivative);
						rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
						if( rc != SQLITE_OK ){
							fprintf(stderr, "SQL error02: %s\n", zErrMsg);
							sqlite3_free(zErrMsg);
						}else{
							//fprintf(stdout, "Database updated successfully\n");
						}
					}
					rc=sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
					cnt_ocu = 0;
					//printf("%d\t %17s %s\n",rssi, address, name); 
					if((deviceCount > 0)/*&&(strstr(name,"XT1092") != NULL)*/){
						rc=sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);						
						sprintf(sql, "select * from BLUETOOTH WHERE time > Datetime('now','localtime','-1 minute') and ADDRESS LIKE '%s'order by time ASC;", address);		
						int count = 0;				
						rssi_db = 0; 
						if ( sqlite3_prepare(db, sql, -1, &statement, 0 ) == SQLITE_OK )
						{
							while(1){
								int res = sqlite3_step(statement);
								if ( res == SQLITE_DONE || res==SQLITE_ERROR){
									break;
								}
								
								if(res == SQLITE_ROW){
									rssi_db = sqlite3_column_int(statement, 4);
									if(cnt_ocu == 0)
										strcpy(time_db_s , sqlite3_column_text(statement, 1));
									if(cnt_ocu > 0){
										strcpy(time_db_f, time_db_s);
										strcpy(time_db_s , sqlite3_column_text(statement, 1));
									}
									derivative_db = sqlite3_column_int(statement, 5);
									strcpy(name , sqlite3_column_text(statement, 2));
									traces[cnt_ocu] = rssi_db;
									if(rssi_db >= -75)
										count++;
								}								
								cnt_ocu++;
							}
							traces[cnt_ocu] = rssi;
							if(rssi >= -75)
								count++; 
							if((cnt_ocu >= 3)&&(difftime(current_time,alert_time)>=20))
							{
								fprintf(pf, "%d\n %d\n %d\n %d\n",traces[cnt_ocu-3],traces[cnt_ocu-2],traces[cnt_ocu-1],traces[cnt_ocu]); 
								if((traces[cnt_ocu]>traces[cnt_ocu-1])&&(traces[cnt_ocu-1]>traces[cnt_ocu-2])&&(traces[cnt_ocu-2]>traces[cnt_ocu-3])){
									alert_time=time(NULL);
									//printf("ALERT\n");  
									udp_bclient();
									fprintf(pf, "ALERT_1\t %s\t %s\t\n",asctime( localtime(&alert_time) ), address);   								alert_sent = 1;
								}
								if((traces[cnt_ocu]>traces[cnt_ocu-1])&&(traces[cnt_ocu-1]>traces[cnt_ocu-2])&&(traces[cnt_ocu-2]<=traces[cnt_ocu-3])){
									alert_time=time(NULL);									//printf("ALERT\n"); 
									udp_bclient();
									fprintf(pf, "ALERT_2\t %s\t %s\t\n",asctime( localtime(&alert_time) ), address);   								alert_sent = 1;
								}	
							}
							if((cnt_ocu >= 2) &&(count >= 1)&&(difftime(current_time,alert_time)>=20)){
								fprintf(pf, "%d\n %d\n %d\n %d\n",traces[cnt_ocu-3],traces[cnt_ocu-2],traces[cnt_ocu-1],traces[cnt_ocu]);								
								alert_time=time(NULL); 
								//printf("ALERT\n");	
								udp_bclient();

								fprintf(pf, "ALERT_3\t %s\t %s\t\n",asctime( localtime(&alert_time) ), address);   									
								alert_sent = 1;
							}			
						}
						int bike_detected = alert_bike();
						if((bike_detected == 1)&&(difftime(current_time,alert_time)>=20)){
							udp_bclient();
							alert_time=time(NULL);						
						}
						sqlite3_finalize(statement);
						rc=sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
						
						//printf("time::::::%lf\n",difftime(current_time,convert(time_db_s)));
						rc=sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
						if((difftime(current_time,convert(time_db_s))>=2.00) /*|| rssi >= -75*/){

							sprintf(sql, "insert into BLUETOOTH (NAME,ADDRESS,RSSI,DERIVATIVE) values ('%s','%s', %d, %d);", name,address,rssi, derivative);
							rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
							if( rc != SQLITE_OK ){
								fprintf(stderr, "SQL error2: %s\n", zErrMsg);
								sqlite3_free(zErrMsg);
							}else{
								//fprintf(stdout, "Inserted successfully\n");
							}
						}

						rc=sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);

						
					}
	
										fclose(fp);				
					break;
				case EVT_INQUIRY_RESULT_WITH_RSSI:
					fp = fopen( rssi_file, "a" );
					info_rssi = (void *)ptr + 1;							
					rssi = info_rssi->rssi;
					ba2str(&info_rssi->bdaddr,address);
					fclose(fp);
					break;
				case EVT_INQUIRY_COMPLETE:
					canceled = 1;
					break;
			}
			fclose(pf);
		
		}
	}
	close(sock);
}

sqlite3_close(db);	
}

int density(){
	sqlite3 *db;
	
	int  rc;
	rc = sqlite3_open("bluetooth_devices.db", &db);
	if( rc ){
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		exit(0);
	}else{
		//fprintf(stdout, "Opened database successfully\n");
	}
	while(1)
	{	

		char sql[1024];
		char address[18];
		char *zErrMsg = 0;
		time_t ltime;
		sqlite3_stmt *statement; 
		
		char devices_in_database[100][18] = {};
		int cnt_dev_in_db = 0;
		
		
		rc=sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
		sprintf(sql, "select * from BLUETOOTH WHERE time > Datetime('now','localtime','-1 minutes')");		
			
		if ( sqlite3_prepare(db, sql, -1, &statement, 0 ) == SQLITE_OK )
		{
			int size = 1;
			while(1){
				int flag = 0;
				int res = sqlite3_step(statement);
				if ( res == SQLITE_DONE || res==SQLITE_ERROR){
					break;
				}
				
				if(res == SQLITE_ROW){
					memset(address,0,sizeof(address));
					strcpy(address, sqlite3_column_text(statement, 3));
					flag = found(address, devices_in_database, size);
					if(!flag){
						strcpy(devices_in_database[cnt_dev_in_db], address);
						
						size++;
						cnt_dev_in_db++;
					}				
				}	
			}
		}
		sqlite3_finalize(statement);
		rc=sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
		//printf("devices %d\n",cnt_dev_in_db);
		int i = 0, rssi_db=0, cnt_ocu=0;
		int num_crossing_pedestrians=0;
		int num_sidewalk_pedestrians=0;
		int num_moving_vehicles=0;
		int num_cars_redlight=0;
		int num_vehicles=0;	
		int traces[20] = { 0 };
		
		for(i=0;i<cnt_dev_in_db;i++){
			

			int flag = 0;
			cnt_ocu = 0;
			int res = 0;

			//printf("yesa**********************************%17s\n", devices_in_database[i]);
			rc=sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
			sprintf(sql, "select * from BLUETOOTH WHERE time > Datetime('now','localtime','-1 minutes') and ADDRESS LIKE '%s' order by time ASC;", devices_in_database[i]);
			if ( sqlite3_prepare(db, sql, -1, &statement, 0 ) == SQLITE_OK )
			{							
								
				while(1){
					res = sqlite3_step(statement);
					if ( res == SQLITE_DONE || res==SQLITE_ERROR){
						break;
					}
				
					if(res == SQLITE_ROW){
						rssi_db = sqlite3_column_int(statement, 4);
						traces[cnt_ocu] = rssi_db;
						if(rssi_db >= -75)
							flag = 1;
										
					}	
					cnt_ocu = cnt_ocu + 1;
			
				}
				
				sqlite3_reset(statement);
				rc=sqlite3_finalize(statement);	

				if( rc != SQLITE_OK ){
				fprintf(stderr, "SQL error4: %s\n", zErrMsg);
				sqlite3_free(zErrMsg);
				}else{
				//fprintf(stdout, "Table Cleared successfully!\n");
				}

				if(cnt_ocu<3)
					num_moving_vehicles++;
				int j = 0;
				if(cnt_ocu>=3){
					int min_rssi=0;
					int max_rssi=-100;
					for(j=0;j<cnt_ocu;j++){
						if(traces[j] < min_rssi)
							min_rssi = traces[j];
						if(traces[j] > max_rssi)
							max_rssi = traces[j];
						j++;
					}
					int max_var=0;
					max_var = max_rssi - min_rssi;
					if(max_var <= 3)
						num_cars_redlight++;
					if(max_var > 3){
						int sw = 1;
						if(flag == 1){
							num_crossing_pedestrians++;
							sw = 0;
						}
						else{
							while(cnt_ocu >= 3){
								if((traces[cnt_ocu]>traces[cnt_ocu-1])&&(traces[cnt_ocu-1]>traces[cnt_ocu-2])&&(traces[cnt_ocu-2]>traces[cnt_ocu-3])){		
									num_crossing_pedestrians++;
									sw = 0;
							}
								if((traces[cnt_ocu]>traces[cnt_ocu-1])&&(traces[cnt_ocu-1]>traces[cnt_ocu-2])&&(traces[cnt_ocu-2]<=traces[cnt_ocu-3])){		
									num_crossing_pedestrians++;
									sw = 0;
							}
								cnt_ocu--;
							}
							if (sw == 1)
								num_sidewalk_pedestrians++;
						}
						
					}
				}
			}				
			rc=sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
			
			
		
		}
		printf("In the last one minute\n");
		printf("# Cars stopped by the red light %d\n",num_cars_redlight);
		printf("# Moving vehicles %d\n",num_moving_vehicles);
		printf("# Pedestrians on sidewalk %d\n",num_sidewalk_pedestrians);
		printf("# Pedestrians Crossing the road %d\n",num_crossing_pedestrians);
		printf("\n");	

		rc=sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
		sprintf(sql, "DELETE from BLUETOOTH WHERE time <= Datetime('now','localtime','-1 minutes')");
		rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
		if( rc != SQLITE_OK ){
		fprintf(stderr, "SQL error3: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		}else{
		//fprintf(stdout, "Table Cleared successfully!\n");
		}
		rc=sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
		publish(1,0,5,2);
		
		sleep(60);
	}
	sqlite3_close(db);
	
	

}
int main(int argc, char **argv)
{
	if(argc != 3){
		printf("Number of Arguments NOT 3!\n");
		exit(2);	
	}
	pid_t child1, child2;
	if (!(child1 = fork())) {
		Inquiry_Start(argv[1], argv[2]);
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
