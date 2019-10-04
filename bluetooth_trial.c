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

/*printf("year: %d; month: %d; day: %d;\n",
        tm.tm_year, tm.tm_mon, tm.tm_mday);
printf("hour: %d; minute: %d; second: %d\n",
        tm.tm_hour, tm.tm_min, tm.tm_sec);
printf("week day: %d; year day: %d\n", tm.tm_wday, tm.tm_yday);*/


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

int found(char *address, char traces[20][18]){
	int flag = 0, i = 0;
	
	for (i = 0; i < 20 ; i++){
		if(strcmp(address,traces[i]) == 0){
			flag = 1;
			ind = i;
		}
	}

	
	return flag;
}


static void Inquiry_start(char *filename, char *rssi_file)
{

while(1)
{
	
	int dev_id, sock = 0, signal, rssi;

	char name[248] = { 0 };
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

	
	/*inquiry_cp cp;
	memset (&cp, 0, sizeof(cp));
	cp.lap[2] = 0x9e;
	cp.lap[1] = 0x8b;
	cp.lap[0] = 0x33;
	cp.num_rsp = 0x00;
	cp.length = 0x08;

	if (hci_send_cmd(sock, OGF_HOST_CTL, OCF_WRITE_INQUIRY_MODE, WRITE_INQUIRY_MODE_RP_SIZE, &cp) < 0) {
			perror("Can't set inquiry mode");
			return;
	}*/

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
			time_t ltime,current_time; 

			pf = fopen( filename, "a" );
			int flag_current = 0;
			int deviceCount = 0;
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
					strncpy(name,info_extended->data,9);
					ba2str(&info_extended->bdaddr,address);
					sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
					current_time = time(NULL);
					double k;
					k = difftime(current_time,0.00);
					fprintf(fp,"%d\t %17s\t %d\t %s\t\n",info_extended->rssi, address, (int) k, name);
	
					/*Check if the device already exists in the data base*/
					sprintf(sql, "select count(address) from BLUETOOTH WHERE time > Datetime('now','-1 minute') and ADDRESS LIKE '%s';", address);
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
					printf("Device Number in the database:%d\n", deviceCount);
					if(deviceCount==0){
						sprintf(sql, "insert into BLUETOOTH (NAME,ADDRESS,RSSI) values ('%s','%s', %d);", name,address,rssi);
						rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
						if( rc != SQLITE_OK ){
							fprintf(stderr, "SQL error2: %s\n", zErrMsg);
							sqlite3_free(zErrMsg);
						}else{
							fprintf(stdout, "Database updated successfully\n");
						}
					}
					
					rssi_avg = 0;
					cnt_ocu = 0;
					printf("%d\t %17s %s\n",rssi, address, name); 
					if((deviceCount != 0)/*&&(strstr(name,"XT1092") != NULL)*/){
						sprintf(sql, "select * from BLUETOOTH WHERE time > Datetime('now','-1 minute') and ADDRESS LIKE '%s'order by time ASC;", address);
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
									//printf("time_db %s\n", time_db_f);
									strcpy(name , sqlite3_column_text(statement, 2));
									traces[cnt_ocu] = rssi_db;
								}
								//printf("diff: %lf\n",difftime(convert(time_db_s),convert(time_db_f)));
								
								if(cnt_ocu == 0){
									rssi_avg = rssi_db;		
								}
								if(cnt_ocu <= 2&&cnt_ocu >0 && (difftime(convert(time_db_s),convert(time_db_f))>2)){
									rssi_avg = (rssi_avg*(cnt_ocu+1) + rssi_db)/(cnt_ocu + 2);
								}
								if(cnt_ocu > 2&&(difftime(convert(time_db_s),convert(time_db_f))>2)){
									rssi_avg = (rssi_avg*(cnt_ocu + 1) + rssi_db)/(cnt_ocu + 2);
									if(/*(alert_sent == 0)&&*/(rssi_avg >= -87)&&(traces[cnt_ocu] - traces[cnt_ocu - 1] > 1))						{	
										ltime=time(NULL); 
										alert_sent = 1;
										//udp_bclient();
										fprintf(pf, "ALERT2\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);  						
									}													
									if(/*(alert_sent == 0)&&*/(rssi_avg < -87)&&(traces[cnt_ocu] - traces[cnt_ocu-1] > 3))							{
										ltime=time(NULL); 
										fprintf(pf, "ALERT3\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);  
										alert_sent = 1;			
									}
								}
								cnt_ocu++;
							}
							if(cnt_ocu > 2 && (difftime(time(NULL),convert(time_db_s))>2)){
								
								rssi_avg = (rssi_avg*(cnt_ocu + 1) + rssi)/(cnt_ocu + 2);

								if(/*(alert_sent == 0)&&*/(rssi_avg >= -87)&&(abs(rssi - traces[cnt_ocu - 1])> 1))								{	
									ltime=time(NULL); 
									alert_sent = 1;
									fprintf(pf, "ALERTLa\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);  						
								}													
								if(/*(alert_sent == 0)&&*/(rssi_avg < -87)&&(rssi - traces[cnt_ocu - 1] > 3))								{
									ltime=time(NULL); 
									fprintf(pf, "ALERT3\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);  
									alert_sent = 1;			
								}
							}
							fprintf(pf,"%d\t %s\t %17s\n",rssi, name, address);
							fprintf(pf,"end scan procedure\n");				
						}
						sqlite3_finalize(statement);
						
						printf("time::::::%lf\n",difftime(current_time,convert(time_db_s))); 
						if(difftime(current_time,convert(time_db_s))>3.00){
						sprintf(sql, "insert into BLUETOOTH (NAME,ADDRESS,RSSI) values ('%s','%s', %d);", name,address,rssi);
						rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
						if( rc != SQLITE_OK ){
							fprintf(stderr, "SQL error2: %s\n", zErrMsg);
							sqlite3_free(zErrMsg);
						}else{
							//fprintf(stdout, "Database updated successfully\n");
						}
					}
						
									
					}
					
					
					
					sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
					fclose(fp);				
					break;
				case EVT_INQUIRY_RESULT_WITH_RSSI:
					fp = fopen( rssi_file, "a" );
					info_rssi = (void *)ptr + 1;							
					rssi = info_rssi->rssi;
					ba2str(&info_rssi->bdaddr,address);
					if((rssi>-65) /*&& (alert_sent == 0)*/)
					{		
						ltime=time(NULL);
						fprintf(pf, "Alert\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);
						//alert_sent = 1;
					}
					flag_current = found(address, devices_in_cur_scan);
					printf("flag: %d\n", flag_current);
					if(!flag_current){
						strcpy(devices_in_cur_scan[cnt_dev_in_cur_scan], address);
						cnt_dev_in_cur_scan++;
					}
					
					sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL); 

					
					
					/*Check if the device already exists in the data base*/
					sprintf(sql, "select count(address) from BLUETOOTH WHERE time > Datetime('now','-1 minute') and ADDRESS LIKE '%s';", address);
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
					fprintf(fp,"%d\t %17s\n",info_rssi->rssi, address); 
					
					if((deviceCount == 0)){
						/*if (hci_read_remote_name(sock, &(info_rssi)->bdaddr, sizeof(name),
				 		 name, 0) < 0){
							strcpy(name,"[unknown]");
						}*/
						
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
					
					rssi_avg = 0;
					//traces[20] = { 0 };
					cnt_ocu = 0;
					
					if(!flag_current && (deviceCount != 0)){
						sprintf(sql, "select * from BLUETOOTH WHERE time < Datetime('now','-1 minute') and ADDRESS LIKE '%s'order by time ASC;", address);
						rssi_db = 0; 
						if ( sqlite3_prepare(db, sql, -1, &statement, 0 ) == SQLITE_OK )
						{
							while(1){
								int res = sqlite3_step(statement);
								if ( res == SQLITE_DONE || res==SQLITE_ERROR){
									//fprintf(pf,"done from database..................\n");
									break;
								}
								
								if(res == SQLITE_ROW){
									rssi_db = sqlite3_column_int(statement, 4);
									strcpy(name , sqlite3_column_text(statement, 2));
									if(strcmp(name, "[unknown]")==0){
										//if (hci_read_remote_name(sock, &(info_rssi)->bdaddr, sizeof(name), name, 0) < 0)

											strcpy(name,"[unknown]");
																			
									}
									traces[cnt_ocu] = rssi_db;
								}
								fprintf(pf," %d\t %s\t %17s\n",rssi_db, name, address);
								
								
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
								if(cnt_ocu == 0){
									rssi_avg = rssi_db;		
								}
								if(cnt_ocu <= 2 && cnt_ocu >0 ){
									rssi_avg = (rssi_avg*(cnt_ocu+1) + rssi_db)/(cnt_ocu + 2);
								}
								//fprintf(pf,"count %d\n",cnt_ocu);
								if(cnt_ocu > 2){
									rssi_avg = (rssi_avg*(cnt_ocu + 1) + rssi_db)/(cnt_ocu + 2);
									//printf("rssi_avg %f\n", rssi_avg);
									if(/*(alert_sent == 0)&&*/(rssi_avg >= -87)&&(traces[cnt_ocu] - traces[cnt_ocu - 1] > 1))						{	
										ltime=time(NULL); 
										alert_sent = 1;
										//udp_bclient();
										fprintf(pf, "ALERT2\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);  						
									}													
									if(/*(alert_sent == 0)&&*/(rssi_avg < -87)&&(traces[cnt_ocu] - traces[cnt_ocu-1] > 3))							{
										ltime=time(NULL); 
										fprintf(pf, "ALERT3\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);  
										alert_sent = 1;			
									}
								}
								cnt_ocu++;
								//fprintf(pf,"not done from database..................\n");
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
							}
							
							//cnt_ocu++;
							if(cnt_ocu > 2){
								
								rssi_avg = (rssi_avg*(cnt_ocu + 1) + rssi)/(cnt_ocu + 2);
								//fprintf(pf,"rssi_last_in_db %d\n", cnt_ocu);

								if(/*(alert_sent == 0)&&*/(rssi_avg >= -87)&&(rssi - traces[cnt_ocu - 1] > 1))								{	
									ltime=time(NULL); 
									alert_sent = 1;
									fprintf(pf, "ALERTLa\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);  						
								}													
								if(/*(alert_sent == 0)&&*/(rssi_avg < -87)&&(rssi - traces[cnt_ocu - 1] > 3))								{
									ltime=time(NULL); 
									fprintf(pf, "ALERT3\t %s\t %s\t\n",asctime( localtime(&ltime) ), address);  
									alert_sent = 1;			
								}
							}
							fprintf(pf,"%d\t %s\t %17s\n",rssi, name, address);
							fprintf(pf,"end scan procedure\n");				
						}
						sqlite3_finalize(statement);
						print_result(&info_rssi->bdaddr, 1, info_rssi->rssi, name);
						sprintf(sql, "insert into BLUETOOTH (NAME,ADDRESS,RSSI) values ('%s','%s', %d);", name,address,rssi);
						rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
						if( rc != SQLITE_OK ){
							fprintf(stderr, "SQL error2: %s\n", zErrMsg);
							sqlite3_free(zErrMsg);
						}else{
							//fprintf(stdout, "Database updated successfully\n");
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
		sql = "select count(address) from BLUETOOTH where time < Datetime('now','-1 minute') and rssi > -75";
		sqlite3_stmt *statement;
		if ( sqlite3_prepare(db, sql, -1, &statement, 0 ) == SQLITE_OK ) {
			int ctotal = sqlite3_column_count(statement);
			 								
			while(ctotal>=0){							
				int res = sqlite3_step(statement);
				if(res == SQLITE_ROW){
					density = sqlite3_column_int(statement, 0);
					//printf("density %d\n",density);
					ltime=time(NULL); 
					//fprintf(fp,"%d\t %s\n",density, asctime( localtime(&ltime) ));
				}
				ctotal--;
				
				if ( res == SQLITE_DONE || res==SQLITE_ERROR){
					break;
				}
			}
		}
		sqlite3_finalize(statement);
		fclose(fp);
		sql = "DELETE from BLUETOOTH WHERE time > Datetime('now','-1 minute')";
		rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
		if( rc != SQLITE_OK ){
		fprintf(stderr, "SQL error3: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		}else{
		fprintf(stdout, "Table Cleared successfully!\n");
		}
		rc=sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
		sqlite3_close(db);
		sleep(70);
	}
	
	

}
int main(int argc, char **argv)
{
	if(argc != 3){
		printf("Number of Aeguments NOT 3!\n");
		exit(2);	
	}
	pid_t child1, child2;
	if (!(child1 = fork())) {
		Inquiry_start(argv[1], argv[2]);
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
