#include "gpio.h"

//#define DEBUG_FILE
//#define WDOGLED
#define TEST_READ_GPIO

FILE *fpDebug= NULL;

// Mutex for thread synchronization
pthread_mutex_t lock;
// Thread ID for GPIO read and LCD menu functionalities
pthread_t tidGpioLcd;

struct sigaction act;

unsigned char showVersion = 0x01;
unsigned char gpioOutAlv = 0x00;

/****************************************************************
 *Function Name: sigkill_handler
 *Description:   Handle a signal receive during program
 *               terminated by kill command
 *Return:        NA
 *
 ***************************************************************/
void sigkill_handler(int signum, siginfo_t *info, void *ptr)
{
    info = NULL;
    ptr = NULL;

	exit (0);
}

/****************************************************************
 *Function Name: sigint_handler
 *Description:   Handle a signal receive during program
 *               terminated by CTRL + C
 *Return:        NA
 *
 ***************************************************************/
void sigint_handler(int signum)
{

	exit (0);
}

/****************************************************************
 *Function Name: get_curr_time
 *Description:   Retrieve controller current time
 *Return:        Current time in HH:MM:SS format
 *
 ***************************************************************/
char *get_curr_time (void)
{
	time_t t ;
    struct tm *tmp ;

	char timebuf[80];
	char *ptimebuf = malloc(80);

	memset(&timebuf, 0, sizeof(timebuf));

    time(&t);

    //localtime() uses the time pointed by t ,
    // to fill a tm structure with the
    // values that represent the
    // corresponding local time.
    tmp = localtime(&t);

    // using strftime to display time
	// Retrieve local time - HH:MM:SS
	strftime(timebuf, sizeof(timebuf), "%T", tmp);
	// Copy time buffer to the return pointer
	for (int i = 0; i < 80; i++)
	{
		// As long as not empty
		if (timebuf[i] != 0x00)
		{
			ptimebuf[i] = timebuf[i];
		}
		// Put end of string character
		else
		{
			ptimebuf[i] = '\0';
			break;
		}
	}
	return ptimebuf;
}

/****************************************************************
 *Function Name: get_curr_date
 *Description:   Retrieve controller current date
 *Return:        Current time in D/M/Y format
 *
 ***************************************************************/
char *get_curr_date (void)
{
	time_t t ;
    struct tm *tmp ;

	char datebuf[80];
	char *pdatebuf = malloc(80);

	memset(&datebuf, 0, sizeof(datebuf));

    time(&t);

    //localtime() uses the time pointed by t ,
    // to fill a tm structure with the
    // values that represent the
    // corresponding local time.
    tmp = localtime(&t);

    // using strftime to display time
	// Retrieve local date - D/M/Y
	strftime(datebuf, sizeof(datebuf), "%d/%m/%Y", tmp);
	// Copy date buffer to the return pointer
	for (int i = 0; i < 80; i++)
	{
		// As long as not empty
		if (datebuf[i] != 0x00)
		{
			pdatebuf[i] = datebuf[i];
		}
		// Put end of string character
		else
		{
			pdatebuf[i] = '\0';
			break;
		}
	}
	return pdatebuf;
}

/****************************************************************
 *Function Name: delay_sec
 *Description:   Delay in seconds with configurable interval
 *
 *Return:        NA
 ***************************************************************/
void delay_sec( int seconds )
{
	clock_t endwait;

	endwait = clock () + seconds * CLOCKS_PER_SEC;
	while (clock() < endwait) {}
}

/****************************************************************
 *Function Name: msleep
 *Description:   Delay in miliseconds with configurable interval
 *
 *Return:        NA
 ***************************************************************/
int msleep(unsigned int tms)
{
  return usleep(tms * 1000);
}

/****************************************************************
 *Function Name: thread_lcd_menu
 *Description:   Thread to read 2 GPIO and LCD menu navigation
  *Return:        NA
 *
 ***************************************************************/
void *thread_lcd_menu (void *arguments)
{
    arguments = NULL;

    char *pCurrDate;
	char *pCurrTime;

	unsigned char gpioUp = 0x00;
    unsigned char gpioDwn = 0x00;

    pCurrDate = get_curr_date(); // Get current date
	pCurrTime = get_curr_time(); // Get current time

    // Initialize GPIO50 - 0x50
    // Initialize failed!
    if (read_gpio(0x50,0,gpioUp) == 0xff)
    {
        printf ("[%s %s] THD-LCD-MENU: UP GPIO initialization FAILED!\n", pCurrDate, pCurrTime);
    }
    // Initialize successfull
    else
    {
        // Only enable and initialize this GPIO once
        if (gpioUp == 0x00)
        {
            gpioUp = 0x01;
        }
        printf ("[%s %s] THD-LCD-MENU: UP GPIO initialization SUCCESSFULL\n", pCurrDate, pCurrTime);
    }
    // Initialize GPIO50 - 0x50
    // Initialize failed!
    if (read_gpio(0x52,0,gpioDwn) == 0xff)
    {
        printf ("[%s %s] THD-LCD-MENU: DOWN GPIO initialization FAILED!\n", pCurrDate, pCurrTime);
    }
    // Initialize successfull
    else
    {
        // Only enable and initialize this GPIO once
        if (gpioDwn == 0x00)
        {
            gpioDwn = 0x01;
        }
        printf ("[%s %s] THD-LCD-MENU: DOWN GPIO initialization SUCCESSFULL\n", pCurrDate, pCurrTime);
    }
    // Release memory allocation
	free(pCurrDate);
	free(pCurrTime);
	// Thread main loop
    while(1)
    {
        pCurrDate = get_curr_date(); // Get current date
        pCurrTime = get_curr_time(); // Get current time

        // Test GPIO for UP and DOWN menu
        #ifdef TEST_READ_GPIO
        // READ UP GPIO
        // ON
        if (read_gpio(0x50,0,gpioUp) == 0x00)
        {
            printf ("[%s %s] THD-LCD-MENU: UP GPIO ON\n", pCurrDate, pCurrTime);
        }
        // OFF
        else
        {
            printf ("[%s %s] THD-LCD-MENU: UP GPIO OFF\n", pCurrDate, pCurrTime);
        }
        // READ DOWN GPIO
        // ON
        if (read_gpio(0x52,0,gpioDwn) == 0x00)
        {
            printf ("[%s %s] THD-LCD-MENU: DOWN GPIO ON\n", pCurrDate, pCurrTime);
        }
        // OFF
        else
        {
            printf ("[%s %s] THD-LCD-MENU: DOWN GPIO OFF\n", pCurrDate, pCurrTime);
        }
        #endif

        // Release memory allocation
        free(pCurrDate);
        free(pCurrTime);
        sleep(1);
    }
    pthread_exit(NULL);
    return NULL;
}

/****************************************************************
 *Function Name: main
 *Description:   Daemon main program entry point
 *
 *Return:        NA
 ***************************************************************/
int main()
{
	int clearlogcnt = 0;
	int verCnt = 0;
	int lcdCnt = 0;
	int spaceCnt = 0;

	unsigned char aliveONOFF = 0x00;
	unsigned char lcdDateTime = 0x01;

	char *pCurrDate;
	char *pCurrTime;
	char datetimebuff[16];

	memset(&act, 0, sizeof(act));
	memset(&datetimebuff, 0, sizeof(datetimebuff));

	act.sa_sigaction = sigkill_handler;
	act.sa_flags = SA_SIGINFO;

	sigaction(SIGTERM, &act, NULL);  // For handling signal generated by kill command
	signal(SIGINT, sigint_handler);  // For handling signal generated by interrupt ctrl-c
	signal(SIGSEGV, sigint_handler); // For handling signal generated by segmentation violation
	signal(SIGUSR1, sigint_handler); // For handling kill -USR1 action, for KILL the daemon manually

    // Initialize mutex for thread synchronization
	if (pthread_mutex_init(&lock, NULL) != 0)
	{}

	#ifdef DEBUG_FILE
	// Change the current working directory to root.
	if ((chdir("/")) < 0)
	{
		exit(EXIT_FAILURE); // Return failure
	}
	// Open a log file in write mode.
	fpDebug = fopen ("iointerface.info", "w+");

	#endif // DEBUG_FILE

	pCurrDate = get_curr_date(); // Get current date
	pCurrTime = get_curr_time(); // Get current time

	// Initialize FINTEK GPIO API
	// Initialization successfull
	if (init_fintek_gpio() == 0x01)
	{
	    #ifdef DEBUG_FILE
		fprintf(fpDebug, "[%s %s] MAIN-LOOP: GPIO initialization SUCCESSFULL\n", pCurrDate, pCurrTime);
		#else
		printf ("[%s %s] MAIN-LOOP: GPIO initialization SUCCESSFULL\n", pCurrDate, pCurrTime);
		#endif // DEBUG_FILE
	}
	// Initialization failed
	else
	{
		#ifdef DEBUG_FILE
		fprintf(fpDebug, "[%s %s] MAIN-LOOP: GPIO initialization FAILED!\n", pCurrDate, pCurrTime);
		#else
		printf ("[%s %s] MAIN-LOOP: GPIO initialization FAILED!\n", pCurrDate, pCurrTime);
		#endif // DEBUG_FILE

		exit(EXIT_FAILURE); // Return failure
	}
	//write_gpio(0x54,1,0,0,gpioOutAlv);

	// Initialize LCD function
	// RS pin - GPIO54
	// Enable pin - GPIO56
	// Data pin (D4-D7) - GPIO51, GPIO53, GPIO55, GPIO57
	if (LiquidCrystal_init(0x54, 0x56, 0x51, 0x53, 0x55, 0x57) == 0xff)
	{
		#ifdef DEBUG_FILE
		fprintf(fpDebug, "[%s %s] MAIN-LOOP: LCD initialization FAILED!\n", pCurrDate, pCurrTime);
		#else
		printf ("[%s %s] MAIN-LOOP: LCD initialization FAILED!\n", pCurrDate, pCurrTime);
		#endif // DEBUG_FILE
	}
	else
	{
		begin(16, 2); // Initialize LCD character and rows

		#ifdef DEBUG_FILE
		fprintf(fpDebug, "[%s %s] MAIN-LOOP: LCD initialization SUCCESSFULL\n", pCurrDate, pCurrTime);
		#else
		printf ("[%s %s] MAIN-LOOP: LCD initialization SUCCESSFULL\n", pCurrDate, pCurrTime);
		#endif // DEBUG_FILE
	}

	clear(); // Clear first LCD previous contents
	setCursor (0, 0);
	printlcd ("   MASURI+SCH   ", 16);
	setCursor(0, 1);
	printlcd (" Version: 1.0.1 ", 16);

    // Create thread for GPIO read and LCD menu functionalities
    // Failed
    if (pthread_create(&tidGpioLcd,NULL,thread_lcd_menu,NULL) < 0)
    {
        printf ("[%s %s] MAIN-LOOP: Create thread GPIO and LCD menu FAILED!\n", pCurrDate, pCurrTime);
    }
    // Release memory allocation
	free(pCurrDate);
	free(pCurrTime);

    // Main process loop
	// Process interval at the main loop will used custom delay
	mainLoop:
		msleep(500);

		pCurrDate = get_curr_date(); // Get current date
		pCurrTime = get_curr_time(); // Get current time

		#ifdef DEBUG_FILE
		clearlogcnt++;
		fflush(fpDebug);
		// Clear the online log after some times
		if (clearlogcnt == 1000)
		{
			clearlogcnt = 0;
			fclose(fpDebug);

			fpDebug = fopen ("iointerface.info", "w+");
		}
		#endif // DEBUG_FILE
		// Show daemon version number
		if (showVersion)
		{
			verCnt++;
			// Every 30s, print the IO interface daemon version - 30/0.5
			if (verCnt == 60)
			{
				#ifdef DEBUG_FILE
				fprintf(fpDebug, "[%s %s] MAIN-LOOP: IO-Interface Version: 1.0.1\n", pCurrDate, pCurrTime);
				#else
				printf("[%s %s] MAIN-LOOP: IO-Interface Version: 1.0.1\n", pCurrDate, pCurrTime);
				#endif // DEBUG_FILE
				verCnt = 0;
			}
		}
		// Turn ON LED - For controller ALIVE indicator (GPIO50)
		if (aliveONOFF == 0x00)
		{
			aliveONOFF = 0x01;

			#ifdef WDOGLED
			// Turn ON LED - For controller ALIVE indicator (GPIO50)
			// Failed
			if (write_gpio(0x50,1,0,0,gpioOutAlv) == 0xff)
			{
				#ifdef DEBUG_FILE
				fprintf(fpDebug, "[%s %s] MAIN-LOOP: ALIVE ON Failed!\n", pCurrDate, pCurrTime);
				#else
				printf ("[%s %s] MAIN-LOOP: ALIVE ON Failed!\n", pCurrDate, pCurrTime);
				#endif // DEBUG_FILE
			}
			// Successfull
			else
			{
				// Only enable and initialize this GPIO once
				if (gpioOutAlv == 0x00)
				{
					gpioOutAlv = 0x01;
				}
			}
			#endif
		}
		// Turn OFF LED - For controller ALIVE indicator (GPIO50)
		else
		{
			aliveONOFF = 0x00;

			#ifdef WDOGLED
			// Turn OFF LED - For controller ALIVE indicator (GPIO50)
			// Failed
			if (write_gpio(0x50,1,0,1,gpioOutAlv) == 0xff)
			{
				#ifdef DEBUG_FILE
				fprintf(fpDebug, "[%s %s] MAIN-LOOP: ALIVE OFF Failed!\n", pCurrDate, pCurrTime);
				#else
				printf ("[%s %s] MAIN-LOOP: ALIVE OFF Failed!\n", pCurrDate, pCurrTime);
				#endif // DEBUG_FILE
			}
			// Successfull
			else
			{
				// Only enable and initialize this GPIO once
				if (gpioOutAlv == 0x00)
				{
					gpioOutAlv = 0x01;
				}
			}
			#else
			lcdCnt++; // Seconds counter

			// Every 5s change LCD display contents
			// Shows current date and time
			if (lcdCnt == 10)
			{
				lcdDateTime = 0x00;

				clear(); // Clear first LCD previous contents
				setCursor (0, 0);
				printlcd ("   MASURI+SCH   ", 16);
				setCursor(0, 1);
				printlcd (" Version: 1.0.1 ", 16);
			}
			else if (lcdCnt == 20)
			{
				lcdDateTime = 0x01;
				lcdCnt = 0;
			}
			// Display time on the LCD
			if (lcdDateTime == 0x01)
			{
				setCursor (0, 0);

				memset(&datetimebuff, 0, sizeof(datetimebuff));

				// Stupid method to arrange date to display on the LCD
				// xx/xx/yyyy
				datetimebuff[0]   = ' ';
				datetimebuff[1]   = ' ';
				datetimebuff[2]   = ' ';
				datetimebuff[3]   =  pCurrDate[0];  // "D"
				datetimebuff[4]   =  pCurrDate[1];  // "D"
				datetimebuff[5]   =  pCurrDate[2];  // "/"
				datetimebuff[6]   =  pCurrDate[3];  // "M"
				datetimebuff[7]   =  pCurrDate[4];  // "M"
				datetimebuff[8]   =  pCurrDate[5];  // "/"
				datetimebuff[9]   =  pCurrDate[6];  // "Y"
				datetimebuff[10]  =  pCurrDate[7];  // "Y"
				datetimebuff[11]  =  pCurrDate[8];  // "Y"
				datetimebuff[12]  =  pCurrDate[9];  // "Y"
				datetimebuff[13]  = ' ';
				datetimebuff[14]  = ' ';
				datetimebuff[15]  = ' ';

				printlcd (datetimebuff, 16);

				setCursor(0, 1);

				// Stupid method to arrange time to display on the LCD
				// xx:xx:xx
				datetimebuff[0]   = ' ';
				datetimebuff[1]   = ' ';
				datetimebuff[2]   = ' ';
				datetimebuff[3]   = ' ';
				datetimebuff[4]   =  pCurrTime[0];  // "H"
				datetimebuff[5]   =  pCurrTime[1];  // "H"
				datetimebuff[6]   =  pCurrTime[2];  // ":"
				datetimebuff[7]   =  pCurrTime[3];  // "M"
				datetimebuff[8]   =  pCurrTime[4];  // "M"
				datetimebuff[9]   =  pCurrTime[5];  // ":"
				datetimebuff[10]  =  pCurrTime[6];  // "S"
				datetimebuff[11]  =  pCurrTime[7];  // "S"
				datetimebuff[12]  = ' ';
				datetimebuff[13]  = ' ';
				datetimebuff[14]  = ' ';
				datetimebuff[15]  = ' ';

				printlcd (datetimebuff, 16);

				memset(&datetimebuff, 0, sizeof(datetimebuff));
			}
			#endif
		}
		// Release memory allocation
		free(pCurrDate);
		free(pCurrTime);
	goto mainLoop;

	return 0;
}
