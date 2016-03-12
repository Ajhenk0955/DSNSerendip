/*
Data Analysis Code v1.0 6/17/2005
2005 Daniel Chapman and Andrew Siemion
SETI group
Space Sciences Lab
University of California, Berkeley

This program analyzes the binary data files created by the receive code.

Usage: <program> [filename] [options]
 -e <error checking>  Enable error checking.
 -w <write to file>  Write packets to text file.
 -g <graph>  Plot the data.

  [filename] is the name of the files minus the "_<integer>.dat" to be analyzed.
First the file "[filename]_1.dat" will be analyzed, then "[filename]_2.dat", and so on
until no file is found.

Outputs.
   If error checking is enabled, errors will be reported as output to the screen.
   If write to file is enabled, files will be copied from binary format to text format
   with the same filename but with the extension ".txt" in place of ".dat"

  
   
-------------- IN DEPTH EXPLANATION ----------------------

Each frequency spectrum consists of 4096 polyphase filter bank bins (PFB bins).  Each PFB bin consists of
32768 bins, which means that each spectrum consists of 134217728 (4096*32678) individual bins.

THE UDP CODE --------------------------------------------------

The UDP code spits UDP packets from the Bee2.  A packet reports all of the information
regarding one particular PFB bin.  The first three numbers in a packet describe the PFB bin.
The first number is the PFB bin number, the second number is the mean power, and
the third number is an error code that is reported from the Bee2 (0 means no error).
After the first three numbers are reported, every hit in the specified PFB bin is reported.
Each hit consist of two numbers: which individual bin the hit belongs to, and the power of
the hit.  Each number is 4 bytes

To summarize, the data field of a UDP packet looks like this:
PFB bin number
mean power
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

THE RECEIVE CODE ---------------------------------------------

The receive code receives these UDP packets from the Bee2.  It then either writes the data to a<cr>
file or graphs the data.  If the write to file options is chosen, it writes to file a header
for each packet and the data from the packet.  The header consists of 3 numbers.  The first
number is the size of the data field in bytes, and the last two numbers are the seconds and
microseconds of the time stamp from when the packet was received (time since January 1st, 1970).
Each number is 4 bytes.

The packets are written to file in binary form, each packet representing a PFB bin number.
A file is closed and the next begun when it has as many packets in it as was specified in
program execution.  This means that a packet (PFB bin) will never be split up across files,
but a spectrum may be split up and continued into the next file.

To summarize, the data written to file looks like this:
length of data
seconds field of time stamp
microseconds field of time stamp
data (PFBbinNumber, meanPower, errorCode, hit #1 bin number, ...)

The filenames will be based on the [filename] parameter entered at the execution of the receive
code.  Each filename will consist of [filename], a time stamp (seconds since January 1st, 1970),
the character '_', an integer (a counter for each file), and lastly the extension '.dat'

For example, if I executed the code right now with "./receive -w -f gobears", the files would be named:
gobears1117039029_1.dat
gobears1117039029_2.dat
gobears1117039029_3.dat
...

THE DATA ANALYSIS CODE ------------------------------------

The data analysis code parses every file in a set.  The filename given as a parameter to the
program is the name of the file excluding the "_<number>.dat".  For the files in the example
above, the user would enter "gobears1117039029" as the filename parameter.  Then each file,
beginning with "gobears1117039029_1.dat", would be parsed until the next file in the series
cannot be found.

If write to file is enabled, for every ".dat" file a new ".txt" file will be created with
otherwise the same name, and the data files will be copied into the new files in text format.
The two numbers representing the time stamp (<sec> and <usec>) will be combined into one
number of the form '<sec>.<usec>' and the packets will be separated by an extra new line.

If error checking is enabled any error code other than 0 will be reported, as well as any
missing PFB bin number.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "/home/danw/SPECTROSUITE/gnuplot_i-2.10/src/gnuplot_i.h"

void setTitle();
void quit();
void togglePaused();
void toggleHits();
void toggleLogPlot();
void toggleKeyCommands();
void zoomOut();

int endianSwap32(int x);

#define FFT_OVERFLOW_MASK 0x20000000
#define PFB_OVERFLOW_MASK 0x10000000
#define CT_ERROR_MASK     0x0F000000
#define FIFO_OVERRUN_MASK 0x00FFC000

#define INTSIZE 10
#define ERROR_CHECK 0
#define PARSE 0
#define BYTE_SWAPPING 0
#define MAXHITS 424288


gnuplot_ctrl *h1;

int paused = 0;
int plotHits = 1;
int keyCommands = 0;
int domainBin = 0;
int domainAdjustedFrequency = 1;
char frequencyAdjustmentCmd[100];

/* To be used for malloc..
double *hitbins;
double *hitpowers;
*/


int main (int argc, const char * argv[]) {

	int errorChecking=0;
	int writingToTextFile=0;
	int plotting=0;
	
	FILE *fp;
	FILE *fpToWrite;
	int numberOfFiles;
	char filename[100];
	char filenameToWrite[100];
	char fileheader[100];
	int counter;
	int numberOfMissingPFBbins;
	int numberOfErrorCodesReported;
	
	
	
	int i, j;
	char *datebuf;
	

	int firstPFBbinNumberFlag=1;
	unsigned int bytesOfData, sec, usec, lastSec, lastUSec, PFBbinNumber, lastPFBbinNumber, meanPower, errorCode;
	char dummy[1036];

	char pauseCommand[100];
	char zoomOutCommand[100];
	char quitCommand[100];
	char toggleHitsCommand[100];
	char toggleLogPlotCommand[100];
	char toggleKeyCommandsCommand[100];	

	double binsLeft[2049];
	double avgpowerLeft[2049];
	double binsRight[2049];
	double avgpowerRight[2049];
	double binsConnection[2];
	double avgpowerConnection[2];
	
	//Reduced to meet dave's basic needs
	double hitbins[MAXHITS];
	double hitpowers[MAXHITS];

	int frequencyCenter = 2275;

	int processID=0;

	int totalHits = 0;
	int totalBins = 0;
	int lastbin = -1;
	int pktcountLeft = 0;
	int pktcountRight = 0;	
        
        time_t atime;
	unsigned int curavgpower, currentbin, currentPFBbin, tempbin, temppower;

	/* Worthless malloc code
	hitbins = (double *)malloc(MAXHITS * 8);
	hitpowers = (double *)malloc(MAXHITS *8);
	*/



	/* analyze flags */
	if(argc < 2){
		printf("Usage: %s [filename] [options]\n", argv[0]);
		printf(" -g <plotting>\n");
		printf(" -e <error checking>\n");
		printf(" -w <write to file>  Write packets to text file.\n");
		quit();
	}
	else{
	  //strcpy(fileheader, argv[1]);  Worse comes to worse my options come first

		for(i=1; i<argc; i++){
			if(strcmp(argv[i], "-e") == 0){
				errorChecking = 1;
			}
			else if(strcmp(argv[i], "-w") == 0){
				writingToTextFile = 1;
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
			else strcpy(fileheader, argv[i]);
		}
	}
	
	
    	

	

	strcat(fileheader, "_");

	if(errorChecking){
		printf("Error checking enabled.\n");
	}
	if(writingToTextFile){
		printf("Writing to text file enabled.\n");
	}
	if(plotting){
		printf("Plotting enabled.\n");
	}

	/* find the number of files */
	counter=1;
	
	sprintf(filename, "%s%i.dat", fileheader, counter);
	fp = fopen(filename, "rb");
	
	if(fp != NULL){
	        fread(&bytesOfData, sizeof(int), 1, fp);
	        fread(&sec, sizeof(int), 1, fp);
		fread(&usec, sizeof(int), 1, fp);
		atime = (time_t) sec;
		
		datebuf = ctime(&atime);

	}


	while(fp != NULL){		
		fclose(fp);
		counter++;
		sprintf(filename, "%s%i.dat", fileheader, counter);
		fp = fopen(filename, "rb");
	}
	numberOfFiles = counter-1;
	if(numberOfFiles == 1){
		printf("Found 1 file.\n");
	}
	else{
		printf("Found %i files.\n", numberOfFiles);	
	}
	
	if((numberOfFiles == 0) || (!errorChecking && !writingToTextFile && !plotting)){
		quit();
	}



	printf("It appears data collection began on %s", datebuf);
	



//	if(errorChecking){
		numberOfMissingPFBbins=0;
		numberOfErrorCodesReported=0;
		lastSec = 0;
		lastUSec = 0;
		lastPFBbinNumber = 0;
//	}

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
		gnuplot_cmd(h1, "set style line 6 lt 3 lw 1");         // Build style for average power graph
		gnuplot_cmd(h1, "set style line 5 lt 1 pt 6 ps 1");    // Build style for hit points

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


	/* parse the files */
	for(i=1; i<=numberOfFiles; i++){
		printf("\nParsing file %i------------------------------------\n", i);
		sprintf(filename, "%s%i.dat", fileheader, i);
		fp = fopen(filename, "rb");
		
		if(writingToTextFile){
			sprintf(filenameToWrite, "%s%i.txt", fileheader, i);
			fpToWrite = fopen(filenameToWrite, "w");
			if(fpToWrite == NULL){
				printf("Error: could not open %s to write to.\n", filenameToWrite);
			}
		}
		
		if(fp != NULL){

			/* obtain first packet information */
			fread(&bytesOfData, sizeof(int), 1, fp);
			fread(&sec, sizeof(int), 1, fp);
			fread(&usec, sizeof(int), 1, fp);

			while(!feof(fp)){
			if(!paused){
				if(writingToTextFile && (fpToWrite != NULL)){
					fprintf(fpToWrite, "%i\n", bytesOfData);
					fprintf(fpToWrite, "%i.%06i\n", sec, usec);
				}
				
				/* read the PFBbinNumber */
				if(errorChecking){
					lastPFBbinNumber = PFBbinNumber;
				}
				fread(&PFBbinNumber, sizeof(int), 1, fp);
				if(BYTE_SWAPPING){
					PFBbinNumber = endianSwap32(PFBbinNumber);
				}
				if(writingToTextFile && (fpToWrite !=NULL)){
					fprintf(fpToWrite, "%i\n", PFBbinNumber);
				}
				if(errorChecking){
					if(firstPFBbinNumberFlag){
						lastPFBbinNumber = (PFBbinNumber-1)%4096;
						lastSec = sec;
						lastUSec = sec;
						firstPFBbinNumberFlag = 0;
					}

					if((PFBbinNumber > 4095) || (PFBbinNumber < 0)){
						printf("PFBbinNumber %i out of range (0 to 4095) at time %i.%i\n", PFBbinNumber, sec, usec);
					}
					else if(PFBbinNumber <= lastPFBbinNumber){
						if((lastPFBbinNumber <= 4095) && (lastPFBbinNumber >= 0)){
						if((PFBbinNumber != 0) || (lastPFBbinNumber != 4095)){
							printf("Missing PFB bins between time %i.%i and %i.%i:\n", lastSec, lastUSec, sec, usec);
							for(j=lastPFBbinNumber+1; j<4096; j++){
								printf("     missing PFBbinNumber %i\n", j);
								numberOfMissingPFBbins++;
							}
							for(j=0; j<PFBbinNumber; j++){
								printf("     missing PFBbinNumber %i\n", j);
								numberOfMissingPFBbins++;
							}
						}
						}
					}
					else if((PFBbinNumber - lastPFBbinNumber) != 1){
						printf("Missing PFB bins between time %i.%i and %i.%i:\n", lastSec, lastUSec, sec, usec);
						for(j=lastPFBbinNumber+1; j<PFBbinNumber; j++){
							printf("     missing PFBbinNumber %i\n", j);
							numberOfMissingPFBbins++;
						}
					}
				}

				if(plotting){
					currentbin = PFBbinNumber;
					currentPFBbin = currentbin;
					currentPFBbin = (currentPFBbin + 2048) % 4096;

					if((signed int) currentbin <= lastbin){
						totalBins = pktcountLeft+pktcountRight-2;
						gnuplot_resetplot(h1);
						if(plotHits){
							printf("Plot: lastbin: %i, currentbin: %i, totalBins: %i, totalHits: %i.\n", lastbin, currentbin, totalBins, totalHits);
							gnuplot_setstyle(h1, "points ls 5");
							gnuplot_plot_xy(h1, hitbins, hitpowers, totalHits, "Hits");

						}
						else{
							printf("Plot: currentbin: %i, totalBins: %i, totalHits: %i. (Hits are off).\n", currentbin, totalBins, totalHits);
						}
						gnuplot_setstyle(h1, "steps ls 6");
						binsConnection[0] = binsLeft[pktcountLeft-1];
						binsConnection[1] = binsRight[0];						
						avgpowerConnection[0] = avgpowerLeft[pktcountLeft-1];
						avgpowerConnection[1] = avgpowerRight[0];

						gnuplot_plot_xy(h1, binsLeft, avgpowerLeft, pktcountLeft, "");
						gnuplot_plot_xy(h1, binsRight, avgpowerRight, pktcountRight, "");
						gnuplot_plot_xy(h1, binsConnection, avgpowerConnection, 2, "");

						usleep(500000);

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
					
				}



				
				/* read the mean power */
				fread(&meanPower, sizeof(int), 1, fp);
				if(BYTE_SWAPPING){
					meanPower = endianSwap32(meanPower);
				}
				if(writingToTextFile && (fpToWrite !=NULL)){
					fprintf(fpToWrite, "%i\n", meanPower);
				}
				if(plotting){
					lastbin = currentbin;
					curavgpower = meanPower;
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




				/* read the error code */
				fread(&errorCode, sizeof(int), 1, fp);
				if(BYTE_SWAPPING){
					errorCode = endianSwap32(errorCode);
				}
				if(writingToTextFile && (fpToWrite !=NULL)){
					fprintf(fpToWrite, "%i\n", errorCode);
				}
				if(errorChecking){
					if((errorCode & FFT_OVERFLOW_MASK) > 0){
						printf("FFT overflow reported in PFBbinNumber %i\n", PFBbinNumber);
						numberOfErrorCodesReported++;
					}
					if((errorCode & PFB_OVERFLOW_MASK) > 0){
						printf("PFB overflow reported in PFBbinNumber %i\n", PFBbinNumber);
						numberOfErrorCodesReported++;
					}
					if((errorCode & CT_ERROR_MASK) > 0){
						printf("Corner Turner error reported in PFBbinNumber %i\n", PFBbinNumber);
						numberOfErrorCodesReported++;
					}
					if((errorCode & FIFO_OVERRUN_MASK) > 0){
						printf("FIFO overrun reported in PFBbinNumber %i\n", PFBbinNumber);
						numberOfErrorCodesReported++;
					}
				}
				
				/* read the rest of the data */
				if(writingToTextFile || plotting){
					for(j=0; j<(bytesOfData-12)/8; j++){
						fread(&tempbin, sizeof(int), 1, fp);
						fread(&temppower, sizeof(int), 1, fp);						
						if(BYTE_SWAPPING){
							tempbin = endianSwap32(tempbin);
							temppower = endianSwap32(temppower);							
						}
						if(writingToTextFile && (fpToWrite !=NULL)){
							fprintf(fpToWrite, "%i\n", tempbin);
							fprintf(fpToWrite, "%i\n", temppower);						
						}
						if(plotting){

							if(domainBin){
								hitbins[totalHits]  = ((double) ((((tempbin) + 16384) % 32768) + 32768*currentPFBbin)) / 32768.0 - 0.5;
							}
							else if(domainAdjustedFrequency){
								hitbins[totalHits]  = ((double) ((((tempbin) + 16384) % 32768) + 32768*currentPFBbin)) / 671088.64 - 100.0 - 0.0244140625 + (double)frequencyCenter;
							}
							else{
								printf("Internal error: No domain preference.\n");
								hitbins[totalHits]  = (double) ((((tempbin) + 16384) % 32768) + 32768*currentPFBbin);
							}
							hitpowers[totalHits] = ((double) temppower) / 2147483648.0;
							totalHits += 1;
						}
					}
				}
				else{
					fread(dummy, sizeof(dummy[0]), bytesOfData-12, fp);
				}

				
				/* Separate packets with a new line ??   ------------------------ DECISION TO BE MADE ----------------- */
				if(writingToTextFile && (fpToWrite !=NULL)){
					fprintf(fpToWrite, "\n");
				}
				/* obtain next packet information */
				if(errorChecking){
					lastSec = sec;
					lastUSec = usec;
				}
				fread(&bytesOfData, sizeof(int), 1, fp);
				fread(&sec, sizeof(int), 1, fp);
				fread(&usec, sizeof(int), 1, fp);
			}
			}
			fclose(fp);
			if(writingToTextFile && (fpToWrite !=NULL)){
				fclose(fpToWrite);
			}
//		}
		}
	}
	
	if(errorChecking){
		printf("\nFinal report:\n");
		printf("Missing %i PFB bins\n", numberOfMissingPFBbins);
		printf("%i error codes reported\n", numberOfErrorCodesReported);
	}
		
    return 0;
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


void quit(){
        
        /* for malloc, maybe
        free(hitbins);
        free(hitpowers);
        */

	exit(0);	
}


void setTitle()
{
	char title[16384];
	int replot = 1;
	strcpy(title, "set title \"                   UC Berkeley / JPL  SETI BEE2 Spectrometer");
	if(!paused && !(!plotHits)){
		strcat(title, "                   ");
		replot = 0;
	}
	else if(!paused && (!plotHits)){
		strcat(title, " - HITS OFF        ");
		replot = 0;
	}
	else if(paused && !(!plotHits)){
		strcat(title, " - PAUSED          ");
	}
	else if(paused && (!plotHits)){
		strcat(title, " - PAUSED, HITS OFF");
	}
	if(keyCommands){
		strcat(title, "\\n(Press k to hide key commands)\\nZ: zoom out  A: auto-scale  P: previous zoom  N: next zoom\\nK: toggle key commands  X: toggle pause  C: toggle hits  L: toggle log plot  9: quit\"");
	}
	else{
		strcat(title, "\\n(Press k to show key commands)\"");
	}

	gnuplot_cmd(h1, title);
	if(replot){
		gnuplot_cmd(h1, "replot");
	}
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
			strcat(word, "0.1");
		}
		strcat(word, ":");
		if(ymax <= 0){
			strcat(word, "0.2");
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
