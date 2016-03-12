/*
Receive Code v6.0 7/27/2005
2005 Andrew Siemion and Daniel Chapman
SETI group
Space Sciences Lab
University of California, Berkeley

To compile: gcc -O -Wall -o myprogram gnuplot_i.c myprogram.c -lm

This program receives UDP packets and either writes each packet, each packet's size, and a time stamp
to binary files or plots the data it receives.

Usage: receive [options]
-g                     Plot the data in real time via GNUPlot.
-w                     Write the data to a binary file.
-p <port number>       Specify port number (default is 2010.)
-v                     Output greater quantities of program status information.
-x <spectra per file>  Specify number of spectra per file.
-r <files to record>   Specify number of files to record.
-f <file name / path>  Specify file prefix and -optionally- where logfiles are
                       generated.
-ft <file name / path> Specify file prefix and -optionally- where logfiles are
                       generated, appending a time stamp to <file name / path>.
-s <ip address>        Specify ip address (including dots) of network interface
                       receiving packets. (default is 192.168.0.2) 
-t                     Test mode, self-generate packetized data and send to
                       localhost.
-th <threshold>        Specify the threshold scaler as a float in range
                       [0.0, 255.0] (Default 0.09375).
-el <event limit>      Specify the event limit as an integer in range [1, 256]
                       (Default 128).
-d1                    Graphs by PFB bin number.
-d2 <center>           Graph by frequency, specify the center frequency.
                       (Default, default center 2275.)
-port0                 Change serial port connection to the BEE2 from the
                       default port /dev/ttyS1 to /dev/ttyS0.
-N                     For use from within Matlab/Octave. Write 2nd spectrum
                       to file.
-h (or any other garbage) -- Get this help.
** Use file name /dev/stdout for screen output.

Outputs (if writing to file):
  Files named <filename>_<integer>.dat, where <filename> is the parameter following -f,
and <integer> is incremented for each new file, starting with 1.  A new file begins when
the last file has <spectra per file> spectra, where <spectra per file> is the parameter
following -x.  If the -f option is not supplied, a timestamp will be used.

 
-------------- IN DEPTH EXPLANATION ----------------------

Each frequency spectrum consists of 4096 polyphase filter bank bins (PFB bins).  Each PFB bin consists of
32768 bins, which means that each spectrum consists of 134217728 (4096*32678) individual bins.

THE UDP CODE (Run on the Bee 2) --------------------------------------------------

The UDP code spits UDP packets from the Bee2.  A packet reports all of the information
regarding one particular PFB bin.  The first three numbers in a packet describe the PFB bin.
The first number is the PFB bin number, the second number is the threshold, and
the third number is an error code that is reported from the Bee2 (0 means no error).
After the first three numbers are reported, every hit in the specified PFB bin is reported.
Each hit consist of two numbers: which individual bin the hit belongs to, and the power of
the hit.  Each number is 4 bytes

To summarize, the data field of a UDP packet looks like this:
PFB bin number
threshold
error code
hit #1 bin number
hit #1 power
hit #2 bin number
hit #2 power
hit #3 bin number
hit #3 power
....

There is a maximum number of hits per PFB bin that will be reported (all others will be ignored).
This number has yet to be determined, but is temporarily set to 128.

THE RECEIVE CODE (Run on a recipient machine) ---------------------------------------------

The receive code receives these UDP packets from the Bee2.  It then either writes the data to a
file or graphs the data.  If the write to file options is chosen, it writes to file a header
for each packet and the data from the packet.  The header consists of 3 numbers.  The first
number is the size of the data field in bytes, and the last two numbers are the seconds and
microseconds of the time stamp from when the packet was received (time since January 1st, 1970).
Each number is 4 bytes.

The packets are written to file in binary form, each packet representing a PFB bin number.
A file is closed and the next begun when it has as many packets in it as was specified in
program execution.  This means that a packet (PFB bin) will never be split up across files,
but a spectrum may be split up and continued into the next file.

To summarize, the data written to file for each packet looks like this:
length of data
seconds field of time stamp
microseconds field of time stamp
data (PFBbinNumber, threshold, errorCode, hit #1 bin number, ...)

The filenames will be based on the [filename] parameter entered at the execution of the receive
code.  Each filename will consist of the [filename] parameter given at the time of execution followed by 
an underscore and an integer.  The integer value begins at 1 and counts upwards as multiple files are written.
For example, if I executed the code right now with "./receive -w -f gobears", the files would be named:

gobears_1.dat
gobears_2.dat
gobears_3.dat

Without the -f option, a timestamp will be substituted for [filename].
...

THE DATA ANALYSIS CODE ------------------------------------

The data analysis code parses every file in a set.  The filename given as a parameter to the
program is the name of the file excluding the "_<number>.dat".  For the files in the example
above, the user would enter "gobears" as the filename parameter.  Then each file,
beginning with "gobears_1.dat", would be parsed until the next file in the series
cannot be found.

If write to file is enabled, for every ".dat" file a new ".txt" file will be created with
otherwise the same name, and the data files will be copied into the new files in text format.
The two numbers representing the time stamp (<sec> and <usec>) will be combined into one
number of the form '<sec>.<usec>' and the packets will be separated by an extra new line.

If error checking is enabled any error code other than 0 will be reported, as well as any
missing PFB bin number.
*/



#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <sys/timex.h>
#include "/home/danw/SPECTROSUITE/gnuplot_i-2.10/src/gnuplot_i.h"
#include <unistd.h> /* close() */
#include <string.h> /* memset() */
#include <fcntl.h>

#define MAX_MSG 1060
#define INTSIZE 10
#define BYTE_SWAPPING 1
#define MAXHITS 424288

void setTitle();
void quit();
void togglePaused();
void toggleHits();
void toggleLogPlot();
void toggleKeyCommands();
void zoomOut();
void print_usage(const char *prog_name);
void parse_args(int argc, const char** argv);
int endianSwap32(int x);
int searchForPercent(int fd);

// Required by ntptime
typedef struct ntptimeval_s{
	struct timeval time;
	long int maxerror;
	long int esterror;
}ntptimeval;

FILE *fp;
char fileheader[120];
time_t timestuff;
gnuplot_ctrl *h1;

//Defaults
int filesToWrite = 1;
int spectraPerFile = 1;
int plotting = 0;
int writing = 0;
int crudeoutput = 0;
int verboseflag = 0;
int domainBin = 0;
int domainAdjustedFrequency = 1;
int frequencyCenter = 2275.0;
char frequencyAdjustmentCmd[100];
int LOCAL_SERVER_PORT = 2010;
int filepacketsize=100000;
char ServerIP[100]="192.168.0.2";
int paused=0;
int plotHits=1;
int keyCommands=0;
int processID=0;
int spectrum2 = 0;
int testMode = 0;
float threshold = 0.09375;
char port[11] = "/dev/ttyS1";
int eventLimit = 128;

int main (int argc, const char * argv[]) {

	int i, j;
	ntptimeval times;


	char result[1024];
	char toSend[1024];
	int sd, rc, numBytes, cliLen, index;
	int lastbin = -1;
	int lastPFBbin = -1;
	int pktcountLeft = 0;
	int pktcountRight = 0;	
	struct sockaddr_in cliAddr, servAddr;
	char msg[MAX_MSG];
	int twelve = 12;
                                                                                                                             
	//Graphing Arrays to be passed to GNUPlot
	double binsLeft[2049];
	double avgpowerLeft[2049];
	double binsRight[2049];
	double avgpowerRight[2049];
	double binsConnection[2];
	double avgpowerConnection[2];

	//Reduced to meet requirements of dave
	double hitbins[MAXHITS];
	double hitpowers[MAXHITS];

	int PFBmask[4096];
	int filesWritten = 0;
	int numberOfSpectra;
	int skipNextReceive = 0;
	int numfilecounter = 1;
	int totalHits = 0;
	int totalBins = 0; 

	unsigned int curavgpower, currentbin, currentPFBbin, tempbin, temppower;
	char fileheader2[100];
	char fileheader3[100];
	char pauseCommand[100];
	char zoomOutCommand[100];
	char quitCommand[100];
	char toggleHitsCommand[100];
	char toggleLogPlotCommand[100];
	char toggleKeyCommandsCommand[100];	
	FILE *fpToWrite;
	char lastChar;
	int fd;
	
	time(&timestuff);
	sprintf(fileheader, "%i", (int)timestuff);

	if(argc >= 2){
  		parse_args(argc, argv);
	}
	else if(argc == 1){
		print_usage(argv[0]);
		quit();
	}
	if(!writing && !plotting && !spectrum2){
	        printf("\nWhat should I do with this data?\n\n");
		print_usage(argv[0]);
		quit();
	}

	/* Set the PFB mask */
	fpToWrite = fopen("/etc/PFBmask.txt", "rt");
	if(fpToWrite == NULL){
		printf("Could not find PFB mask file /etc/PFBmask.txt.\n");
		for(i=0; i<4096; i++){
			PFBmask[i] = 1;
		}
	}
	else{
		for(i=0; i<4096; i++){
			fscanf(fpToWrite, "%i", &j);
			PFBmask[i] = j;
		}
		fclose(fpToWrite);
	}

	/* Set the threshold level on the BEE2 */
	fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
//	fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
//	fd = open("/dev/ttyS0", O_RDWR);	
	if (fd == -1){
		printf("Warning: Unable to open port %s. Threshold scaler and event limit not set.\n", port);
	}
	else{
		toSend[0] = 3;
		write(fd, toSend, 1);
		i=0;
		if(searchForPercent(fd)){
			write(fd, "`c", 2);
			searchForPercent(fd);
			toSend[0] = 3;
			write(fd, toSend, 1);
			searchForPercent(fd);
			sprintf(toSend, "setscaler %i\n", (int)(threshold * 512));
			write(fd, toSend, strlen(toSend));
			searchForPercent(fd);
			sprintf(toSend, "seteventlimit %i\n", eventLimit);
			write(fd, toSend, strlen(toSend));
			if(searchForPercent(fd) && writing){
				i=0;			
				j=0;
				lastChar = 0;
				write(fd, "boardinfo\n", 11);
				do{
					if((read(fd, &result[i], 1) != -1) && (result[i] != '\r')){
						lastChar = result[i];
//						printf("%c", result[i]);
						i++;
					}
					j++;
				}while((i < 1024) && (j<1000000) && (lastChar != '%'));
				if(j == 1000000){
					i=0;
				}
			}
		}
		else{
			printf("Warning: Problems communicating with BEE2. Threshold scaler and event limit not set.\n");
		}
		if(writing){
			sprintf(fileheader3, "%s.cfg", fileheader);
			fpToWrite = fopen(fileheader3, "w");
			if(fpToWrite == NULL){
				printf("Could not open file %s.\n", fileheader3);
			}
			else{
				if(i > 6+12){
					fwrite("[BOARD INFO]\n", sizeof(char), 13, fpToWrite);
					fwrite(result+12, sizeof(char), i-6-12, fpToWrite);
					fwrite("\n", sizeof(char), 1, fpToWrite);
				}

				sprintf(toSend, "[SPECTRA PER FILE]\n%i\n", spectraPerFile);
				fwrite(toSend, sizeof(char), strlen(toSend), fpToWrite);
				sprintf(toSend, "[FILES TO WRITE]\n%i\n", filesToWrite);
				fwrite(toSend, sizeof(char), strlen(toSend), fpToWrite);
				fwrite("\n[PFB MASK]\n", sizeof(char), 12, fpToWrite);
				for(i=0; i<4096; i++){
					sprintf(toSend, "%i: %i\n", i, PFBmask[i]);
					fwrite(toSend, sizeof(char), strlen(toSend), fpToWrite);
				}
				fclose(fpToWrite);
			}
		}
		close(fd);
	}

	if(testMode){
		memset(ServerIP, 0x0, 100);
		strcpy(ServerIP, "127.0.0.1");
		system("udp localhost -p &");
		testMode = 2;
	}

	signal(SIGHUP, quit);
	signal(SIGINT, quit);
	signal(SIGQUIT, quit);
	signal(SIGALRM, togglePaused);
	signal(60, zoomOut);
	signal(63, quit);
	signal(62, toggleHits);
	signal(61, toggleKeyCommands);
	signal(59, toggleLogPlot);
 

	if(plotting){                           /* Initialize gnuplot window handle and set-up graph   */
        	h1 = gnuplot_init();

		processID = getpid();

		sprintf(pauseCommand, "bind x \"!kill -14 %i\"", processID);
		sprintf(zoomOutCommand, "bind z \"!kill -60 %i\"", processID);
		sprintf(quitCommand, "bind 9 \"!kill -63 %i\"", processID);
		sprintf(toggleHitsCommand, "bind c \"!kill -62 %i\"", processID);
		sprintf(toggleKeyCommandsCommand, "bind k \"!kill -61 %i\"", processID);
		sprintf(toggleLogPlotCommand, "bind l \"!kill -59 %i\"", processID);


		gnuplot_cmd(h1, "set mouse");
		gnuplot_cmd(h1, pauseCommand);
		gnuplot_cmd(h1, zoomOutCommand);
		gnuplot_cmd(h1, quitCommand);
		gnuplot_cmd(h1, toggleHitsCommand);
		gnuplot_cmd(h1, toggleLogPlotCommand);
		gnuplot_cmd(h1, toggleKeyCommandsCommand);
		gnuplot_cmd(h1, "unset key");
		gnuplot_cmd(h1, "set grid");
		gnuplot_cmd(h1, "set pointsize 2");
		gnuplot_cmd(h1, "set style line 6 lt 3 lw 1");         /* Build style for average power graph */
		gnuplot_cmd(h1, "set style line 5 lt 1 pt 6 ps 1");    /* Build style for hit points lt-color, pt-type, ps-size */

		if(domainBin){
			gnuplot_cmd(h1, "set xrange [0:4096]");
		}
		else if(domainAdjustedFrequency){
			sprintf(frequencyAdjustmentCmd, "set xrange [%i:%i]", (frequencyCenter-100), (frequencyCenter+100));
			gnuplot_cmd(h1, frequencyAdjustmentCmd);
		}
		else{
			printf("Internal error: No domain preference.\n");
			gnuplot_cmd(h1, "set xrange [0:134217728]");
		}
		gnuplot_cmd(h1, "set yrange [0.0:0.001]");

		gnuplot_set_ylabel(h1, "Power");


		if(domainBin){
			gnuplot_set_xlabel(h1, "PFB bin number");
		}
		if(domainAdjustedFrequency){
			gnuplot_set_xlabel(h1, "Frequency (MHz)");
		}

		//initialize plotting arrays
		memset(&binsLeft, 0, 2049);
		memset(&avgpowerLeft, 0, 2049);
		memset(&binsRight, 0, 2049);
		memset(&avgpowerRight, 0, 2049);
		memset(&hitbins, 0, MAXHITS);
		memset(&hitpowers, 0, MAXHITS);

		totalHits = 0;
		pktcountLeft = 0;
		pktcountRight = 0;		
		lastbin = -1;
                
		gnuplot_resetplot(h1);
		gnuplot_setstyle(h1, "steps ls 6");
		gnuplot_plot_xy(h1, binsLeft, avgpowerLeft, 1, "Waiting for data...");
		setTitle();
	}

	/* socket creation */
	sd=socket(AF_INET, SOCK_DGRAM, 0);
	if(sd<0) {
		printf("%s: cannot open socket \n",argv[0]);
		quit();
	}
	/* bind local server port */
	servAddr.sin_family = AF_INET;
	if (strcmp(ServerIP, "ANY") == 0){
		servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else {
		servAddr.sin_addr.s_addr = inet_addr(ServerIP);
	} 
	servAddr.sin_port = htons(LOCAL_SERVER_PORT);
	rc = bind (sd, (struct sockaddr *) &servAddr,sizeof(servAddr));
	if(rc<0){
		printf("%s: cannot bind port number %d on network device %s\n", argv[0], LOCAL_SERVER_PORT, ServerIP);
		quit();
	}
	if (verboseflag == 1) {
		printf("%s: waiting for data on network device %s -- port UDP %u\n", argv[0],ServerIP,LOCAL_SERVER_PORT);
	}

	if(writing){
		if(crudeoutput == 0){
			if (verboseflag == 1){
				printf("Filenames will be: %s_<integer>.dat\n", fileheader);
				printf("Maximum packets per file: %i\n", filepacketsize);
			}
			sprintf(fileheader2, "%s_%i.dat", fileheader, numfilecounter);
			fp = fopen(fileheader2, "wb");
			if(fp == NULL){
				printf("Could not open file %s.\n", fileheader2);
			}
		}
		else if(crudeoutput == 1){
			if (verboseflag == 1){
				printf("Outputting to /dev/stdout\n");
			}
			fp = fopen("/dev/stdout", "wb");
		}
	}

	while (1){
		numberOfSpectra = 0;
		while(1){
			if(!skipNextReceive){
				/* init buffer */
				memset(msg,0x0,MAX_MSG);
				/* receive message */
				cliLen = sizeof(cliAddr);
				numBytes = recvfrom(sd, msg, MAX_MSG, 0, (struct sockaddr *) &cliAddr, &cliLen);
			}
			else{
				skipNextReceive = 0;
			}

			
			if(writing){
				
				ntp_gettime((struct ntptimeval *) &times);
				memcpy(&currentbin, &msg, 4);
				if(BYTE_SWAPPING){
					currentbin = endianSwap32(currentbin);
				}
				currentbin = (currentbin + 2048) % 4096;

				if((signed int)currentbin <= lastPFBbin){
					numberOfSpectra++;
					if(numberOfSpectra >= spectraPerFile){
						lastPFBbin = currentbin-1;
						skipNextReceive = 1;
						break;
					}
				}
				lastPFBbin = currentbin;

				if((currentbin >= 0) && (currentbin < 4096) && PFBmask[currentbin]){

					fwrite(&numBytes, sizeof(int), 1, fp);
					fwrite(&times.time.tv_sec, sizeof(int), 1, fp);
					fwrite(&times.time.tv_usec, sizeof(int), 1, fp);
					if(BYTE_SWAPPING){
						for(j=0; j<numBytes/4; j++){
							fwrite(msg+4*j+3, sizeof(char), 1, fp);
							fwrite(msg+4*j+2, sizeof(char), 1, fp);
							fwrite(msg+4*j+1, sizeof(char), 1, fp);
							fwrite(msg+4*j+0, sizeof(char), 1, fp);
						}
					}
					else{
						fwrite(msg, sizeof(char), numBytes, fp);
					}
				}
				else{
					fwrite(&twelve, sizeof(int), 1, fp);
					fwrite(&times.time.tv_sec, sizeof(int), 1, fp);
					fwrite(&times.time.tv_usec, sizeof(int), 1, fp);
					if(BYTE_SWAPPING){
						for(j=0; j<12/4; j++){
							fwrite(msg+4*j+3, sizeof(char), 1, fp);
							fwrite(msg+4*j+2, sizeof(char), 1, fp);
							fwrite(msg+4*j+1, sizeof(char), 1, fp);
							fwrite(msg+4*j+0, sizeof(char), 1, fp);
						}
					}
					else{
						fwrite(msg, sizeof(char), 12, fp);
					}
				}

			}

			if(plotting || spectrum2){
				memcpy(&currentbin, &msg, 4);
				if(BYTE_SWAPPING){
					currentbin = endianSwap32(currentbin);
				}

				currentPFBbin = (currentbin + 2048) % 4096;

				if(PFBmask[currentPFBbin]){
					for(index = totalHits; index < (totalHits+((numBytes-12)/ 8)); index++){
						
						memcpy( &tempbin,    msg + (((index - totalHits)* 8) + 12), 4);
						memcpy( &temppower,  msg + (((index - totalHits)* 8) + 16), 4);
						if(BYTE_SWAPPING){
							tempbin = endianSwap32(tempbin);
							temppower = endianSwap32(temppower);
						}
	
						if(domainBin){
							hitbins[index]  = ((double) ((((tempbin) + 16384) % 32768) + 32768*currentPFBbin)) / 32768.0 - 0.5;
						}
						else if(domainAdjustedFrequency){
							hitbins[index]  = ((double) ((((tempbin) + 16384) % 32768) + 32768*currentPFBbin)) / 671088.64 - 100.0 - 0.0244140625 + (double)frequencyCenter;
						}
						else{
							printf("Internal error: No domain preference.\n");
							hitbins[index]  = (double) ((((tempbin) + 16384) % 32768) + 32768*currentPFBbin);
						}
	
						hitpowers[index] = ((double) temppower) / 2147483648.0;
					}				
					totalHits = totalHits + ((numBytes - 12) / 8);
				}				
				
				if(currentbin <= lastbin){

					if(spectrum2 == 1){
						spectrum2++;
					}
					else if(spectrum2 == 2){
						//  WRITE TO FILE

						fpToWrite = fopen("/tmp/receiveVectorBin","w");
						//write bins, avgpower, hitbins, hitpowers
						for(j=0; j<pktcountLeft-1; j++){
							fprintf(fpToWrite, "%g\t", binsLeft[j]);
						}
						for(j=0; j<pktcountRight-1; j++){
							fprintf(fpToWrite, "%g\t", binsRight[j]);
						}
						fclose(fpToWrite);
						fpToWrite = fopen("/tmp/receiveVectorAvgPower","w");
						for(j=0; j<pktcountLeft-1; j++){
							fprintf(fpToWrite, "%g\t", avgpowerLeft[j]);
						}
						for(j=0; j<pktcountRight-1; j++){
							fprintf(fpToWrite, "%g\t", avgpowerRight[j]);
						}
						fclose(fpToWrite);
						fpToWrite = fopen("/tmp/receiveVectorHitBin","w");
						for(j=0; j<totalHits; j++){
							fprintf(fpToWrite, "%g\t", hitbins[j]);
						}
						fclose(fpToWrite);
						fpToWrite = fopen("/tmp/receiveVectorHitPower","w");
						for(j=0; j<totalHits; j++){
							fprintf(fpToWrite, "%g\t", hitpowers[j]);						
						}
						fclose(fpToWrite);
						quit();
					}
					else if(!paused){
						totalBins = pktcountLeft+pktcountRight;
						gnuplot_resetplot(h1);
						if(totalBins && totalHits){
							if(plotHits){
								printf("Plot: totalBins: %i, totalHits: %i.\n", totalBins, totalHits);
								gnuplot_setstyle(h1, "points ls 5");
								gnuplot_plot_xy(h1, hitbins, hitpowers, totalHits, "Hits");

							}
							else{
								printf("Plot: totalBins: %i, totalHits: %i. (Hits are off).\n", totalBins, totalHits);
							}
							gnuplot_setstyle(h1, "steps ls 6");
							binsConnection[0] = binsLeft[pktcountLeft-1];
							binsConnection[1] = binsRight[0];						
							avgpowerConnection[0] = avgpowerLeft[pktcountLeft-1];
							avgpowerConnection[1] = avgpowerRight[0];
							gnuplot_plot_xy(h1, binsLeft, avgpowerLeft, pktcountLeft, "");
							gnuplot_plot_xy(h1, binsRight, avgpowerRight, pktcountRight, "");
							gnuplot_plot_xy(h1, binsConnection, avgpowerConnection, 2, "");
						}
					}

					lastbin = currentbin;
					totalHits = 0;
					pktcountLeft = 0;
					pktcountRight = 0;					
					memset(&avgpowerLeft, 0, 2049);
					memset(&binsLeft, 0, 2049);
					memset(&avgpowerRight, 0, 2049);
					memset(&binsRight, 0, 2049);
					memset(&hitbins, 0, MAXHITS);
					memset(&hitpowers, 0, MAXHITS);
				}
				lastbin = currentbin;
				memcpy(&curavgpower, msg + 4, 4);
				if(BYTE_SWAPPING){
					curavgpower = endianSwap32(curavgpower);
				}
				// ADJUST FOR THRESHOLDER BINARY POINT, MOVING 32_22 TO 32_31
				//curavgpower <<= 9;

				if(currentPFBbin < 2048){
					/*avgpowerLeft[pktcountLeft] = ((double) curavgpower) / 4194304.0;*/
					avgpowerLeft[pktcountLeft] = ((double) curavgpower) / 2147483648.0;

					if(domainBin){
						binsLeft[pktcountLeft] = ((double) currentPFBbin)-0.5;
					}
					else if(domainAdjustedFrequency){
						binsLeft[pktcountLeft] = (((double) currentPFBbin)-0.5) / 20.48 - 100.0 + (double)frequencyCenter;
					}
					else{
						printf("Internal error: No domain preference.\n");
						binsLeft[pktcountLeft] = (((double) currentPFBbin) - 0.5) * 32768.0;
					}
					pktcountLeft++;
					if(currentPFBbin == 2047 && 0){
						/*avgpowerLeft[pktcountLeft] = ((double) curavgpower) / 4194304.0;*/
						avgpowerLeft[pktcountLeft] = ((double) curavgpower) / 2147483648.0;

						if(domainBin){
							binsLeft[pktcountLeft] = ((double) currentPFBbin)+0.5;
						}
						else if(domainAdjustedFrequency){
							binsLeft[pktcountLeft] = (((double) currentPFBbin)+0.5) / 20.48 - 100.0 + (double)frequencyCenter;
						}
						else{
							printf("Internal error: No domain preference.\n");
							binsLeft[pktcountLeft] = (((double) currentPFBbin) + 0.5) * 32768.0;
						}
						pktcountLeft++;
					}
				}
				else{
					/*avgpowerRight[pktcountRight] = ((double) curavgpower) / 4194304.0;*/
					avgpowerRight[pktcountRight] = ((double) curavgpower) / 2147483648.0;
					
					if(domainBin){
						binsRight[pktcountRight] = ((double) currentPFBbin)-0.5;
					}
					else if(domainAdjustedFrequency){
						binsRight[pktcountRight] = (((double) currentPFBbin)-0.5) / 20.48 - 100.0 + (double)frequencyCenter;
					}
					else{
						printf("Internal error: No domain preference.\n");
						binsRight[pktcountRight] = (((double) currentPFBbin) - 0.5) * 32768.0;
					}
					pktcountRight++;
					if(currentPFBbin == 4095 && 0){
						/*avgpowerRight[pktcountRight] = ((double) curavgpower) / 4194304.0;*/
						avgpowerRight[pktcountRight] = ((double) curavgpower) / 2147483648.0;
						if(domainBin){
							binsRight[pktcountRight] = ((double) currentPFBbin)+0.5;
						}
						else if(domainAdjustedFrequency){
							binsRight[pktcountRight] = (((double) currentPFBbin)+0.5) / 20.48 - 100.0 + (double)frequencyCenter;
						}
						else{
							printf("Internal error: No domain preference.\n");
							binsRight[pktcountRight] = (((double) currentPFBbin) + 0.5) * 32768.0;
						}
						pktcountRight++;
					}
				}
			}
		}
		if(writing && (crudeoutput == 0)){
			filesWritten++;
			if(filesWritten >= filesToWrite){
				quit();
			}

			if (verboseflag == 1) {
				// printf("Number of files written: %i\n", (numfilecounter+1)  );
				printf("Since execution: %i files written, each containing %i packets. \n", numfilecounter, filepacketsize); 
			}
			fclose(fp);
			numfilecounter++;
			sprintf(fileheader2, "%s_%i.dat", fileheader, numfilecounter);
			fp = fopen(fileheader2, "wb");
			if(fp == NULL){
				printf("Could not open file %s.\n", fileheader2);
			}
		}
	}
	return 0;
}




void quit(){
	char fakeudpPID[100];
	FILE *fpToWrite;
	int j;

	if(writing && (fp != NULL)){
		fclose(fp);
	}
	if(testMode == 2){
		fpToWrite = fopen("/tmp/fakeudpPID", "rt");
		if(fpToWrite == NULL){
			printf("Could not kill fakeudp. File /tmp/fakeudpPID not found.");
		}
		else{
			strcpy(fakeudpPID, "kill -9 ");
			j = 8;
			while(!feof(fpToWrite)){
				fakeudpPID[j] = fgetc(fpToWrite);
				j++;
			}
			fakeudpPID[j-1] = '\0';
			fclose(fpToWrite);
			printf("System call: %s\n", fakeudpPID);
			for(j=0; j<200000; j++);
			system(fakeudpPID);
		}
	}

	/*  To be used if malloc becomes implemented
	free(hitbins);
	free(hitpowers);
	*/

	exit(0);	
}


void setTitle()
{
	char title[16384];
	strcpy(title, "set title \"                   UC Berkeley / JPL  SETI BEE2 Spectrometer");
	if(!paused && !(!plotHits)){
		strcat(title, "                   ");
	}
	else if(!paused && (!plotHits)){
		strcat(title, " - HITS OFF        ");
	}
	else if(paused && !(!plotHits)){
		strcat(title, " - PAUSED          ");
	}
	else if(paused && (!plotHits)){
		strcat(title, " - PAUSED, HITS OFF");
	}
	if(keyCommands){
		strcat(title, "\\n(Press k to hide key commands)\\nZ: zoom default  A: auto-scale  P: previous zoom  N: next zoom  [Right Click]: zoom\\nK: toggle key commands  X: toggle pause  C: toggle hits  L: toggle log plot  9: quit\"");
	}
	else{
		strcat(title, "\\n(Press k to show key commands)\"");
	}

	gnuplot_cmd(h1, title);	
	gnuplot_cmd(h1, "replot");
}

void toggleHits()
{
	plotHits = (plotHits+1)%2;
	setTitle();
}

void toggleLogPlot()
{
	static int logScaling=0;
	char word[100];
	float ymin;
	float ymax;
	int changes;
	FILE *fp;
	
	logScaling = (logScaling+1)%2;
	
	gnuplot_cmd(h1, "save set '/tmp/receiveRanges.txt'");
	do{
		fp = fopen("/tmp/receiveRanges.txt", "rt");
	}while(fp == NULL);
	while(strcmp(word, "yrange")!=0){
		fscanf(fp, "%s", word);
	}
	fscanf(fp, "%s", word);
	fscanf(fp, "%f", &ymin);
	fscanf(fp, "%s", word);
	fscanf(fp, "%f", &ymax);
	fclose(fp);
	system("rm /tmp/receiveRanges.txt");		
	changes = 0;
	if((ymax <= 0) || (ymin <= 0)){
		changes=1;
	}
	if(changes){
		strcpy(word, "set yrange [");
		if(ymin <= 0){
			strcat(word, "0.00001");
		}
		strcat(word, ":");
		if(ymax <= 0){
			strcat(word, "0.001");
		}
		strcat(word, "]");
		gnuplot_cmd(h1, word);
	}

	if(logScaling){
		gnuplot_cmd(h1, "set logscale y");
	}
	else{
		gnuplot_cmd(h1, "unset logscale y");
	}
	gnuplot_cmd(h1, "replot");
}

void toggleKeyCommands()
{
	keyCommands = (keyCommands+1)%2;
	setTitle();
}

void togglePaused()
{
	paused = (paused+1)%2;
	setTitle();
}

void zoomOut()
{
	if(domainBin){
		gnuplot_cmd(h1, "set xrange [0:4096]");
	}
	else if(domainAdjustedFrequency){
		gnuplot_cmd(h1, frequencyAdjustmentCmd);
	}
	else{
		printf("Internal error: No domain preference.\n");
		gnuplot_cmd(h1, "set xrange [0:4096]");
	}
	gnuplot_cmd(h1, "set yrange [0.0:0.001]");
	gnuplot_cmd(h1, "replot");
}

void print_usage(const char *prog_name)
{
	printf("Usage: %s [options]\n", prog_name);
	printf(" -g                     Plot the data in real time via GNUPlot.\n");
	printf(" -w                     Write the data to a binary file.\n");
	printf(" -p <port number>       Specify port number (default is 2010.)\n");
	printf(" -v                     Output greater quantities of program status information.\n");
	printf(" -x <spectra per file>  Specify number of spectra per file.\n");
	printf(" -r <files to record>   Specify number of files to record.\n");
	printf(" -f <file name / path>  Specify file prefix and -optionally- where logfiles are \n");
        printf("                        generated.\n");
        printf(" -ft <file name / path> Specify file prefix and -optionally- where logfiles are \n");
        printf("                        generated, appending a time stamp to <file name / path>.\n");	
	printf(" -s <ip address>        Specify ip address (including dots) of network interface\n");
	printf("                        receiving packets. (default is 192.168.0.2)\n");
	printf(" -t                     Test mode, self-generate packetized data and send to\n"); 
	printf("                        localhost.\n");
	printf(" -th <threshold>        Specify the threshold scaler as a float in range\n");
        printf("                        [0.0, 255.0] (Default 0.09375).\n");
	printf(" -el <event limit>      Specify the event limit as an integer in range [1, 256]\n");
        printf("                        (Default 128).\n");
	printf(" -d1                    Graphs by PFB bin number.\n");
	printf(" -d2 <center>           Graph by frequency, specify the center frequency.\n");
        printf("                        (Default, default center 2275.)\n");
	printf(" -port0                 Change serial port connection to the BEE2 from the\n");
	printf("                        default port /dev/ttyS1 to /dev/ttyS0.\n");
	printf(" -N                     For use from within Matlab/Octave. Write 2nd spectrum\n");
	printf("                        to file.\n");
	printf(" -h (or any other garbage) -- Get this help.\n\n");
	printf(" ** Use file name /dev/stdout for screen output.\n");
}  

	
void parse_args(int argc, const char** argv) {

	int i;
	for(i=1;i<argc;i++){
		if(strcmp(argv[i], "-w") == 0){
			writing = 1;
		}
		else if(strcmp(argv[i], "-N") == 0){
			spectrum2 = 1;
		}
		else if(strcmp(argv[i], "-g") == 0){
			plotting = 1;
		}
		else if(strcmp(argv[i], "-d1") == 0){
			domainBin = 1;
			domainAdjustedFrequency = 0;
		}
		else if(strcmp(argv[i], "-d2") == 0){
			domainBin = 0;
			domainAdjustedFrequency = 1;
			if((i+2 <= argc) && (strncmp(argv[i+1], "-", 1) != 0)){
				frequencyCenter = atoi(argv[i+1]);
				i++;
			}
			else{
				frequencyCenter = 0;
			}
		}
		else if(strcmp(argv[i], "-th") == 0){
			i++;
			threshold = atof(argv[i]);
			if((threshold < 0) || (threshold > 255)){
				printf("Invalid threshold scaler value. \nThreshold scaler must be in range 0 to 255.\n");
				quit();
			}
		}
		else if(strcmp(argv[i], "-el") == 0){
			i++;
			eventLimit = atoi(argv[i]);
			if((eventLimit < 1) || (eventLimit > 256)){
				printf("Invalid event limit value. \nEvent limit must be in range 1 to 256.\n");
				quit();
			}
		}
		else if(strcmp(argv[i], "-p") == 0){
			i++;
			LOCAL_SERVER_PORT = atoi(argv[i+1]);
			if((LOCAL_SERVER_PORT <= 999) || (LOCAL_SERVER_PORT >= 62001)){
				printf("Invalid server port. \nServer port must be a value between 1000 and 62000\n");
				quit();
			}
		}
		else if(strcmp(argv[i], "-x") == 0){
			i++;
			spectraPerFile = atoi(argv[i]);
			if((spectraPerFile < 1) || (spectraPerFile > 1000000000)){
				printf("Invalid number of spectra per file. \nSpectra per file must be within the range 1 to 10^9\n");
				quit();
			}
		}
		else if(strcmp(argv[i], "-r") == 0){
			i++;
			filesToWrite = atoi(argv[i]);
			if((filesToWrite < 1) || (filesToWrite > 1000000000)){
				printf("Invalid number of files to write. \nFiles to write must be within the range 1 to 1000000000\n");
				quit();
			}
		}
		else if(strcmp(argv[i], "-f") == 0){
			if(i < argc-1){
				if(strcmp(argv[i+ 1], "/dev/stdout") == 0){
					crudeoutput = 1;
					i++;
				}
				else if (strncmp(argv[i+1], "-", 1) == 0){
					printf("Filenames ought naught begin with a hyphen.\n");           
					quit();
				}
				else{
					
					strcpy(fileheader, argv[ i + 1 ]);
					
					i++;
				}
			}
		}
		else if(strcmp(argv[i], "-ft") == 0){
		        if(i < argc-1){
                        	if(strcmp(argv[i+ 1], "/dev/stdout") == 0){
					crudeoutput = 1;
					i++;
				}
				else if (strncmp(argv[i+1], "-", 1) == 0){
					printf("Filenames ought naught begin with a hyphen.\n");           
					quit();
				}
				else{
				  time(&timestuff);
				  sprintf(fileheader, "%s%i", argv[i+1], (int)timestuff);
				  i++;
				}
			}



		}
		else if(strcmp(argv[i], "-v") == 0){
			verboseflag = 1;
		}
		else if(strcmp(argv[i], "-t") == 0){
			testMode = 1;
		}
		else if(strcmp(argv[i], "-s") == 0){
			strcpy(ServerIP, argv[i+1]);
			i++;
		}
		else if(strcmp(argv[i], "-port0") == 0){
			strcpy(port, "/dev/ttyS0");
		}
		else{
			print_usage(argv[0]);
			printf("%i, %i\n", plotting, writing);
			quit();
		}
	}   
}		

int endianSwap32(int x)
{
	char swapped[4];
	char *pointer = (char *)&x;
	swapped[0] = pointer[3];
	swapped[1] = pointer[2];
	swapped[2] = pointer[1];
	swapped[3] = pointer[0];
	return *(int *)swapped;
}

int searchForPercent(int fd)
{
	char ch;
	int percentSignCounter = 640;
	int connectionCounter = 1000000;
	do{
		if(read(fd, &ch, 1)!=-1){
			percentSignCounter--;
			connectionCounter = 1000000;
		}
		else{
			connectionCounter--;
		}
	}while(connectionCounter && percentSignCounter && (ch != '%'));
	if(connectionCounter && percentSignCounter) return 1;
	else return 0;
}
