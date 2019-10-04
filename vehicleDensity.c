#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

void vehicleDensity(){
	char* file = "/tmp/rxcams.log";
	FILE *fp;
	int numCars = 0;
	int numBikes = 0;
	int id, type;
	double time_now, time_before_car, time_before_bike;
	double time_diff;
	time_t current_time;
	if( access( file, F_OK ) != -1 ){	
		fp = fopen(file, "r");
		rewind(fp); 
		current_time = time(NULL);
		time_diff = difftime(current_time,0.00);

		while((fscanf(fp,"%d %d %lf",&id, &type, &time_now) != EOF)) 
		{
			if(time_now - time_diff > 1000){
				if((type== 5)&&(time_now - time_before_car > 20)){
					numCars++;
					time_before_car = time_now;			
				}		
				if((type == 2)&&(time_now - time_before_bike > 20)){
					numBikes++;
					time_before_bike = time_now;
				}
			}
		}

		fclose(fp);
	}
}

int alert_bike(){

	char* file = "/tmp/rxcams.log";
	FILE *fp;
	int numCars = 0;
	int numBikes = 0;
	int id, type;
	int flag = 0;
	double time_now, time_before_car, time_before_bike;
	double time_diff;
	time_t current_time;
	if( access( file, F_OK ) != -1 ){	
		fp = fopen(file, "r");
		rewind(fp); 
		current_time = time(NULL);
		time_diff = difftime(current_time,0.00);

		while((fscanf(fp,"%d %d %lf",&id, &type, &time_now) != EOF)) 
		{
			if((time_now - time_diff < 1) && (type == 2))
				flag = 1;

		}

		fclose(fp);
	}

	return flag;

}
