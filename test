#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h> 

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
   int i;
   for(i=0; i<argc; i++){
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

int main(int argc, char* argv[])
{
   sqlite3 *db;
   char *zErrMsg = 0;
   int  rc;
   char *sql;

   /* Open database */
   rc = sqlite3_open("bluetooth_devices.db", &db);
   //rc = sqlite3_open("test1.db", &db);
   if( rc ){
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      exit(0);
   }else{
      fprintf(stdout, "Opened database successfully\n");
   }

   /* Create SQL statement */
   sql = "CREATE TABLE BLUETOOTH("  \
         "ID INTEGER PRIMARY KEY   AUTOINCREMENT," \
   //put localtime as default current timestamp
   	 /*"TIME           DEFAULT CURRENT_TIMESTAMP ," \*/
	 "TIME DATE DEFAULT (datetime('now','localtime')),"\
	 "NAME           TEXT    NOT NULL," \
         "ADDRESS        TEXT    NOT NULL," \
	 "RSSI        INT    NOT NULL," \
         "DERIVATIVE           INT     NOT NULL);";
   printf("trace\n");

   /* Execute SQL statement */
   rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
   printf("trace1");
   if( rc != SQLITE_OK ){
   fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   }else{
      fprintf(stdout, "Table created successfully\n");
   }
   sqlite3_close(db);
   return 0;
}
