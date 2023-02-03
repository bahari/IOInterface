#include "gpio.h"

//#define DEBUG_FILE
#define MEN_SELECTED        0x01
#define MEN_NOT_SELECTED    0x00
#define MEN_UPDWN_MAX
#define MEN_UPDWN_MIN       0

FILE *fpDebug= NULL;

// Mutex for thread synchronization
pthread_mutex_t lcdMenuMutex;
// Thread ID for GPIO read and LCD menu functionalities
pthread_t tidGpioLcd;

struct sigaction act;

unsigned char gpioUp = 0x00;
unsigned char gpioDwn = 0x00;
unsigned char showVersion = 0x01;
unsigned char gpioOutAlv = 0x00;
unsigned char gpioUpRead = 0x00;
unsigned char gpioDwnRead = 0x00;
unsigned char mainMenuMode = 0x00;
unsigned char exitMainMenu = 0x00;
unsigned char subMenuMode = 0x00;
unsigned char sipIdComplete = 0x00;

int menuUpDwnCnt = 0;
int mainMenuIdx = 0;
int sipIdStoredCnt = 0;
int sipPwdStoredCnt = 0;
int sipAstIpStoredCnt = 0;

char sipIdStored[5];   // xxxx\0
char sipPwdStored[5];  // xxxx\0
char sipAstIpAddr[16]; // xxx.xxx.xxx.xxx\0

unsigned char pttSetting = 0x00;
unsigned char micSetting = 0x00;
unsigned char audSetting = 0x00;
unsigned char amuSetting = 0x00;

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

    char *pCurrDates;
	char *pCurrTimes;

	printf ("[%s %s] SIGINT: Terminate application via KILL command\n", pCurrDates, pCurrTimes);

    // Clear GPIO API
    deactivate_gpio();

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
    char *pCurrDates;
	char *pCurrTimes;

    printf ("[%s %s] SIGINT: Terminate application via CTRL+C\n", pCurrDates, pCurrTimes);

    // Clear GPIO API
    deactivate_gpio();

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
void delay_sec(int seconds)
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
 *Function Name: storedSIPIpAddr
 *Description:   Stored Asterisk IP address to the buffer
 *
 *Return:        0x00 - Stored IP address process in progress
 *               0x01,0x03 - Stored IP address completed
 *               0x02 - Wrong format of IP address
 *               0x04 - Display current stored IP address
 *               0x05 - Display current partial IP address
 ***************************************************************/
unsigned char storedSIPIpAddr (int astIpAddrNo)
{
    unsigned char retValue = 0x00;
    unsigned char eos = 0x00;
    unsigned char empty = 0x00;
    unsigned char ipSegmOne = 0x00;
    unsigned char ipSegmTwo = 0x00;
    unsigned char ipSegmThree = 0x00;
    unsigned char ipSegmFour = 0x00;
    unsigned char wrongIp = 0x00;

    int floatCnt = 0;
    int ipSegmOneCnt = 0;
    int ipSegmTwoCnt = 0;
    int ipSegmThreeCnt = 0;
    int ipSegmFourCnt = 0;

    char tempBuff[16];
    char dummyBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    // Initialize buffer
    memset(&tempBuff, 0x00, sizeof(tempBuff));

    // SIP password stored and convert operation
    // Only number: 0-9 and float character
    if (astIpAddrNo < 11)
    {
        // Still not reach IP address index
        // Index start = 0, Index end = 15
        if (sipAstIpStoredCnt < 15)
        {
            // Convert number to char
            switch (astIpAddrNo)
            {
                // Number '0'
                case 0:
                    sipAstIpAddr[sipAstIpStoredCnt] = '0';
                break;
                // Number '1'
                case 1:
                    sipAstIpAddr[sipAstIpStoredCnt] = '1';
                break;
                // Number '2'
                case 2:
                    sipAstIpAddr[sipAstIpStoredCnt] = '2';
                break;
                // Number '3'
                case 3:
                    sipAstIpAddr[sipAstIpStoredCnt] = '3';
                break;
                // Number '4'
                case 4:
                    sipAstIpAddr[sipAstIpStoredCnt] = '4';
                break;
                // Number '5'
                case 5:
                    sipAstIpAddr[sipAstIpStoredCnt] = '5';
                break;
                // Number '6'
                case 6:
                    sipAstIpAddr[sipAstIpStoredCnt] = '6';
                break;
                // Number '7'
                case 7:
                    sipAstIpAddr[sipAstIpStoredCnt] = '7';
                break;
                // Number '8'
                case 8:
                    sipAstIpAddr[sipAstIpStoredCnt] = '8';
                break;
                // Number '9'
                case 9:
                    sipAstIpAddr[sipAstIpStoredCnt] = '9';
                break;
                // Float '.'
                case 10:
                    sipAstIpAddr[sipAstIpStoredCnt] = '.';
                break;
            }
            // Stored Asterisk IP address completed
            // xxx.xxx.xxx.xxx
            if (sipAstIpStoredCnt == 14)
            {
                // Check the stored IP address
                // Checking the entered IP address segments
                // xxx.xxx.xxx.xxx
                for (int a = 0; a < 16; a++)
                {
                    // Check for number character
                    if ((sipAstIpAddr[a] == '0') || (sipAstIpAddr[a] == '1') || (sipAstIpAddr[a] == '2')
                        || (sipAstIpAddr[a] == '3') || (sipAstIpAddr[a] == '4') || (sipAstIpAddr[a] == '5')
                        || (sipAstIpAddr[a] == '6') || (sipAstIpAddr[a] == '7') || (sipAstIpAddr[a] == '8')
                        || (sipAstIpAddr[a] == '9'))
                    {
                        if ((!ipSegmOne) && (floatCnt == 0))
                        {
                            ipSegmOneCnt++;
                        }
                        else if ((!ipSegmTwo) && (floatCnt == 1))
                        {
                            ipSegmTwoCnt++;
                        }
                        else if ((!ipSegmThree) && (floatCnt == 2))
                        {
                            ipSegmThreeCnt++;
                        }
                        else if ((!ipSegmFour) && (floatCnt == 3))
                        {
                            ipSegmFourCnt++;
                        }
                    }
                    // Check for float character
                    // Check and confirm all the IP address segments
                    // Format stored: xxx.xxx.xxx.xxx
                    // E.g.: 001.001.001.001
                    else if (sipAstIpAddr[a] == '.')
                    {
                        // Check first IP segments
                        if (floatCnt == 0)
                        {
                            // Correct segment number
                            if ((ipSegmOneCnt < 4) && (ipSegmOneCnt > 0))
                            {
                                // Format IP segments: xxx
                                if (ipSegmOneCnt == 3)
                                {
                                    ipSegmOne = 0x01;
                                }
                                // Wrong IP segments
                                else
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                            // Incorrect segment number
                            else
                            {
                                if ((ipSegmOneCnt >= 4) || (ipSegmOneCnt == 0))
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                        }
                        // Check seconds IP segments
                        else if (floatCnt == 1)
                        {
                            // Correct segment number
                            if ((ipSegmTwoCnt < 4) && (ipSegmTwoCnt > 0))
                            {
                                // Format IP segments: xxx
                                if (ipSegmTwoCnt == 3)
                                {
                                    ipSegmTwo = 0x01;
                                }
                                // Wrong IP segments
                                else
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                            // Incorrect segment number
                            else
                            {
                                if ((ipSegmTwoCnt >= 4) || (ipSegmTwoCnt == 0))
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                        }
                        // Check third IP segments
                        else if (floatCnt == 2)
                        {
                            // Correct segment number
                            if ((ipSegmThreeCnt < 4) && (ipSegmThreeCnt > 0))
                            {
                                // Format IP segments: xxx
                                if (ipSegmThreeCnt == 3)
                                {
                                    ipSegmThree = 0x01;
                                }
                                // Wrong IP segments
                                else
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                            // Incorrect segment number
                            else
                            {
                                if ((ipSegmThreeCnt >= 4) || (ipSegmThreeCnt == 0))
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                        }
                        // Increment float character counter
                        floatCnt++;
                        // Entry of float character are ONLY 3
                        // xxx.xxx.xxx.xxx
                        if (floatCnt > 3)
                        {
                            wrongIp = 0x01;
                            break;
                        }
                    }
                    // Checking last IP address segments
                    else if ((floatCnt == 3) && (ipSegmFourCnt > 0))
                    {
                        // Correct segment number
                        if ((ipSegmFourCnt < 4) && (ipSegmFourCnt > 0))
                        {
                            // Format IP segments: xxx
                            if (ipSegmFourCnt == 3)
                            {
                                ipSegmFour = 0x01;
                            }
                        }
                        // Incorrect segment number
                        else
                        {
                            if ((ipSegmFourCnt >= 4) || (ipSegmFourCnt == 0))
                            {
                                wrongIp = 0x01;
                                break;
                            }
                        }
                    }
                }
                // Wrong IP address segments
                if (wrongIp)
                {
                    // Empty Asterisk IP address buffer and index counter
                    sipAstIpStoredCnt = 0;
                    memset(&sipAstIpAddr, 0x00, sizeof(sipAstIpAddr));

                    setCursor (0, 0);
                    printlcd ("Invalid IP ADDR ", 16);
                    setCursor(0, 1);
                    printlcd ("Segments        ", 16);

                    retValue = 0x02;
                }
                else
                {
                    // Previously all IP address segments are completed
                    if ((ipSegmOne) && (ipSegmTwo) && (ipSegmThree) && (ipSegmFour))
                    {
                        sipAstIpStoredCnt++;
                        sipAstIpAddr[sipAstIpStoredCnt] = '\0';

                        setCursor (0, 0);
                        printlcd ("Stored IP ADDR: ", 16);
                        setCursor(0, 1);
                        printlcd ("Completed       ", 16);

                        retValue = 0x01;
                    }
                }
            }
            // Stored Asterisk IP address in progress
            else
            {
                // Checking the entered IP address segments
                // xxx.xxx.xxx.xxx
                for (int a = 0; a < 16; a++)
                {
                    // Check for number character
                    if ((sipAstIpAddr[a] == '0') || (sipAstIpAddr[a] == '1') || (sipAstIpAddr[a] == '2')
                        || (sipAstIpAddr[a] == '3') || (sipAstIpAddr[a] == '4') || (sipAstIpAddr[a] == '5')
                        || (sipAstIpAddr[a] == '6') || (sipAstIpAddr[a] == '7') || (sipAstIpAddr[a] == '8')
                        || (sipAstIpAddr[a] == '9'))
                    {
                        if ((!ipSegmOne) && (floatCnt == 0))
                        {
                            ipSegmOneCnt++;
                        }
                        else if ((!ipSegmTwo) && (floatCnt == 1))
                        {
                            ipSegmTwoCnt++;
                        }
                        else if ((!ipSegmThree) && (floatCnt == 2))
                        {
                            ipSegmThreeCnt++;
                        }
                        else if ((!ipSegmFour) && (floatCnt == 3))
                        {
                            ipSegmFourCnt++;
                        }
                    }
                    // Check for float character
                    // Check and confirm all the IP address segments
                    // Format stored: xxx.xxx.xxx.xxx
                    // E.g.: 001.001.001.001
                    else if (sipAstIpAddr[a] == '.')
                    {
                        // Check first IP segments
                        if (floatCnt == 0)
                        {
                            // Correct segment number
                            if ((ipSegmOneCnt < 4) && (ipSegmOneCnt > 0))
                            {
                                // Float character enter before IP segments reach 3
                                // Consider error IP segments
                                if (ipSegmOneCnt != 3)
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                            // Incorrect segment number
                            else
                            {
                                if ((ipSegmOneCnt >= 4) || (ipSegmOneCnt == 0))
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                        }
                        // Check seconds IP segments
                        else if (floatCnt == 1)
                        {
                            // Correct segment number
                            if ((ipSegmTwoCnt < 4) && (ipSegmTwoCnt > 0))
                            {
                                // Float character enter before IP segments reach 3
                                // Consider error IP segments
                                if (ipSegmTwoCnt != 3)
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                            // Incorrect segment number
                            else
                            {
                                if ((ipSegmTwoCnt >= 4) || (ipSegmTwoCnt == 0))
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                        }
                        // Check third IP segments
                        else if (floatCnt == 2)
                        {
                            // Correct segment number
                            if ((ipSegmThreeCnt < 4) && (ipSegmThreeCnt > 0))
                            {
                                // Float character enter before IP segments reach 3
                                // Consider error IP segments
                                if (ipSegmThreeCnt != 3)
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                            // Incorrect segment number
                            else
                            {
                                if ((ipSegmThreeCnt >= 4) || (ipSegmThreeCnt == 0))
                                {
                                    wrongIp = 0x01;
                                    break;
                                }
                            }
                        }
                        // Increment float character counter
                        floatCnt++;
                        // Entry of float character are ONLY 3
                        // xxx.xxx.xxx.xxx
                        if (floatCnt > 3)
                        {
                            wrongIp = 0x01;
                            break;
                        }
                    }
                }
                // Wrong IP address segments
                if (wrongIp)
                {
                    // Empty Asterisk IP address buffer and index counter
                    sipAstIpStoredCnt = 0;
                    memset(&sipAstIpAddr, 0x00, sizeof(sipAstIpAddr));

                    setCursor (0, 0);
                    printlcd ("Invalid IP ADDR ", 16);
                    setCursor(0, 1);
                    printlcd ("Segments        ", 16);

                    retValue = 0x02;
                }
                else
                {
                    // Check the IP address segments number entry
                    if ((ipSegmOneCnt >= 4) || (ipSegmTwoCnt >= 4) || (ipSegmThreeCnt >= 4))
                    {
                        // Empty Asterisk IP address buffer and index counter
                        sipAstIpStoredCnt = 0;
                        memset(&sipAstIpAddr, 0x00, sizeof(sipAstIpAddr));

                        setCursor (0, 0);
                        printlcd ("Invalid IP ADDR ", 16);
                        setCursor(0, 1);
                        printlcd ("Segments        ", 16);

                        retValue = 0x02;
                    }
                    else
                    {
                        retValue = 0x00;
                    }
                }
            }
        }
        // Previously Asterisk IP address stored process completed
        else
        {
            setCursor (0, 0);
            printlcd ("Stored IP ADDR: ", 16);
            setCursor(0, 1);
            printlcd ("Completed       ", 16);

            retValue = 0x03;
        }
    }
    // Check stored Asterisk IP address and display to the LCD
    else
    {
        // Display previously stored Asterisk IP address to LCD display - Full Asterisk IP address
        if (sipAstIpStoredCnt == 15)
        {
            // Copy Asterisk IP address to LCD buffer
            // xxx.xxx.xxx.xxx
            for (int a = 0; a < 16; a++)
            {
                // Copy until reach end of string
                if ((sipAstIpAddr[a] != '\0') && (!eos))
                {
                    tempBuff[a] = sipAstIpAddr[a];
                }
                else if ((sipAstIpAddr[a] == '\0') && (!eos))
                {
                    tempBuff[a] = ' ';
                }
                else if (eos)
                {
                    tempBuff[a] = ' ';
                }
            }
            setCursor (0, 0);
            printlcd ("Stored IP ADDR: ", 16);
            setCursor(0, 1);
            printlcd (tempBuff, 16);

            retValue = 0x04;
        }
        // Display previously stored Asterisk IP address to LCD display - Partially stored Asterisk IP address
        else
        {
            // Copy SIP password to LCD buffer
            for (int a = 0; a < 16; a++)
            {
                // Copy until reach empty contents
                if ((sipAstIpAddr[a] != 0x00) && (!empty))
                {
                    tempBuff[a] = sipAstIpAddr[a];
                }
                else if ((sipAstIpAddr[a] == 0x00) && (!empty))
                {
                    tempBuff[a] = ' ';
                    empty = 0x01;
                }
                // Copy empty character to LCD buffer
                else if (empty)
                {
                    tempBuff[a] = ' ';
                }
            }
            setCursor (0, 0);
            printlcd ("Part IP ADDR:   ", 16);
            setCursor(0, 1);
            printlcd (tempBuff, 16);

            retValue = 0x05;
        }
    }
    return retValue;
}

/****************************************************************
 *Function Name: storedSIPPwd
 *Description:   Stored SIP password to the buffer
 *
 *Return:        0x00 - Stored SIP password in progress
 *               0x01,0x02 - Stored SIP password completed
 *               0x03 - Display current stored SIP password
 *               0x04 - Display current partial SIP password
 ***************************************************************/
unsigned char storedSIPPwd (int sipPwdNo)
{
    unsigned char retValue = 0x00;
    unsigned char eos = 0x00;
    unsigned char empty = 0x00;

    char tempBuff[16];

    // Initialize buffer
    memset(&tempBuff, 0, sizeof(tempBuff));

    // SIP password stored and convert operation
    // Only number: 0-9
    if (sipPwdNo < 10)
    {
        // Still not reach SIP password index
        // Index start = 0, Index end = 4
        if (sipPwdStoredCnt < 4)
        {
            // Convert number to char
            switch (sipPwdNo)
            {
                // Number '0'
                case 0:
                    sipPwdStored[sipPwdStoredCnt] = '0';
                break;
                // Number '1'
                case 1:
                    sipPwdStored[sipPwdStoredCnt] = '1';
                break;
                // Number '2'
                case 2:
                    sipPwdStored[sipPwdStoredCnt] = '2';
                break;
                // Number '3'
                case 3:
                    sipPwdStored[sipPwdStoredCnt] = '3';
                break;
                // Number '4'
                case 4:
                    sipPwdStored[sipPwdStoredCnt] = '4';
                break;
                // Number '5'
                case 5:
                    sipPwdStored[sipPwdStoredCnt] = '5';
                break;
                // Number '6'
                case 6:
                    sipPwdStored[sipPwdStoredCnt] = '6';
                break;
                // Number '7'
                case 7:
                    sipPwdStored[sipPwdStoredCnt] = '7';
                break;
                // Number '8'
                case 8:
                    sipPwdStored[sipPwdStoredCnt] = '8';
                break;
                // Number '9'
                case 9:
                    sipPwdStored[sipPwdStoredCnt] = '9';
                break;
            }
            // Stored SIP password completed
            if (sipPwdStoredCnt == 3)
            {
                sipPwdStoredCnt++;
                sipPwdStored[sipPwdStoredCnt] = '\0';

                setCursor (0, 0);
                printlcd ("Stored SIP PWD: ", 16);
                setCursor(0, 1);
                printlcd ("Completed       ", 16);

                retValue = 0x01;
            }
            // Stored SIP password in progress
            else
            {
                retValue = 0x00;
            }
        }
        // Previously SIP Id stored process completed
        else
        {
            setCursor (0, 0);
            printlcd ("Stored SIP PWD: ", 16);
            setCursor(0, 1);
            printlcd ("Completed       ", 16);

            retValue = 0x02;
        }
    }
    // Check stored SIP password and display to the LCD
    else
    {
        // Display previously stored SIP password to LCD display - Full SIP password
        if (sipPwdStoredCnt == 4)
        {
            // Copy SIP password to LCD buffer
            for (int a = 0; a < 16; a++)
            {
                // Copy until reach end of string
                if ((sipPwdStored[a] != '\0') && (!eos))
                {
                    tempBuff[a] = sipPwdStored[a];
                }
                else if ((sipPwdStored[a] == '\0') && (!eos))
                {
                    tempBuff[a] = ' ';
                    eos = 0x01;
                }
                // Copy empty character to LCD buffer
                else if (eos)
                {
                    tempBuff[a] = ' ';
                }
            }
            setCursor (0, 0);
            printlcd ("Stored SIP PWD: ", 16);
            setCursor(0, 1);
            printlcd (tempBuff, 16);

            retValue = 0x03;
        }
        // Display previously stored SIP password to LCD display - Partially stored SIP password
        else
        {
            // Copy SIP password to LCD buffer
            for (int a = 0; a < 16; a++)
            {
                // Copy until reach empty contents
                if ((sipPwdStored[a] != 0x00) && (!empty))
                {
                    tempBuff[a] = sipPwdStored[a];
                }
                else if ((sipPwdStored[a] == 0x00) && (!empty))
                {
                    tempBuff[a] = ' ';
                    empty = 0x01;
                }
                // Copy empty character to LCD buffer
                else if (empty)
                {
                    tempBuff[a] = ' ';
                }
            }
            setCursor (0, 0);
            printlcd ("Part SIP PWD:   ", 16);
            setCursor(0, 1);
            printlcd (tempBuff, 16);

            retValue = 0x04;
        }
    }
    return retValue;
}

/****************************************************************
 *Function Name: storedSIPId
 *Description:   Stored SIP ID to the buffer
 *
 *Return:        0x00 - Stored SIP ID in progress
 *               0x01,0x02 - Stored SIP ID completed
 *               0x03 - Display current stored SIP ID
 *               0x04 - Display current partial SIP ID
 ***************************************************************/
unsigned char storedSIPId (int sipNo)
{
    unsigned char retValue = 0x00;
    unsigned char eos = 0x00;
    unsigned char empty = 0x00;
    char tempBuff[16];

    // Initialize buffer
    memset(&tempBuff, 0, sizeof(tempBuff));

    // SIP ID stored and convert operation
    // Only number: 0-9
    if (sipNo < 10)
    {
        // Still not reach SIP Id index
        // Index start = 0, Index end = 4
        if (sipIdStoredCnt < 4)
        {
            // Convert number to char
            switch (sipNo)
            {
                // Number '0'
                case 0:
                    sipIdStored[sipIdStoredCnt] = '0';
                break;
                // Number '1'
                case 1:
                    sipIdStored[sipIdStoredCnt] = '1';
                break;
                // Number '2'
                case 2:
                    sipIdStored[sipIdStoredCnt] = '2';
                break;
                // Number '3'
                case 3:
                    sipIdStored[sipIdStoredCnt] = '3';
                break;
                // Number '4'
                case 4:
                    sipIdStored[sipIdStoredCnt] = '4';
                break;
                // Number '5'
                case 5:
                    sipIdStored[sipIdStoredCnt] = '5';
                break;
                // Number '6'
                case 6:
                    sipIdStored[sipIdStoredCnt] = '6';
                break;
                // Number '7'
                case 7:
                    sipIdStored[sipIdStoredCnt] = '7';
                break;
                // Number '8'
                case 8:
                    sipIdStored[sipIdStoredCnt] = '8';
                break;
                // Number '9'
                case 9:
                    sipIdStored[sipIdStoredCnt] = '9';
                break;
            }
            // Stored SIP Id completed
            if (sipIdStoredCnt == 3)
            {
                sipIdStoredCnt++;
                sipIdStored[sipIdStoredCnt] = '\0';

                setCursor (0, 0);
                printlcd ("Stored SIP ID:  ", 16);
                setCursor(0, 1);
                printlcd ("Completed       ", 16);

                retValue = 0x01;
            }
            // Stored SIP id in progress
            else
            {
                retValue = 0x00;
            }
        }
        // Previously SIP Id stored process completed
        else
        {
            setCursor (0, 0);
            printlcd ("Stored SIP ID:  ", 16);
            setCursor(0, 1);
            printlcd ("Completed       ", 16);

            retValue = 0x02;
        }
    }
    // Check stored SIP ID and display to the LCD
    else
    {
        // Display previously stored SIP Id to LCD display - Full SIP Id
        if (sipIdStoredCnt == 4)
        {
            // Copy SIP Id to LCD buffer
            for (int a = 0; a < 16; a++)
            {
                // Copy until reach end of string
                if ((sipIdStored[a] != '\0') && (!eos))
                {
                    tempBuff[a] = sipIdStored[a];
                }
                else if ((sipIdStored[a] == '\0') && (!eos))
                {
                    tempBuff[a] = ' ';
                    eos = 0x01;
                }
                // Copy empty character to LCD buffer
                else if (eos)
                {
                    tempBuff[a] = ' ';
                }
            }
            setCursor (0, 0);
            printlcd ("Stored SIP ID:  ", 16);
            setCursor(0, 1);
            printlcd (tempBuff, 16);

            retValue = 0x03;
        }
        // Display previously stored SIP Id to LCD display - Partially stored SIP Id
        else
        {
            // Copy SIP Id to LCD buffer
            for (int a = 0; a < 16; a++)
            {
                // Copy until reach empty contents
                if ((sipIdStored[a] != 0x00) && (!empty))
                {
                    tempBuff[a] = sipIdStored[a];
                }
                else if ((sipIdStored[a] == 0x00) && (!empty))
                {
                    tempBuff[a] = ' ';
                    empty = 0x01;
                }
                // Copy empty character to LCD buffer
                else if (empty)
                {
                    tempBuff[a] = ' ';
                }
            }
            setCursor (0, 0);
            printlcd ("Part SIP ID:    ", 16);
            setCursor(0, 1);
            printlcd (tempBuff, 16);

            retValue = 0x04;
        }
    }
    return retValue;
}

/****************************************************************
 *Function Name: subMenuSIPAstIpStoredStat
 *Description:   Display status of the current Asterisk IP
 *               address stored operation
 *
 *Return:        NA
 ***************************************************************/
void subMenuSIPAstIpStoredStat (int menu)
{
    switch (menu)
    {
        case 0:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Stored Number:0 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 1:
            setCursor (0, 0);
            printlcd ("Stored Number:1 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 2:
            setCursor (0, 0);
            printlcd ("Stored Number:2 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 3:
            setCursor (0, 0);
            printlcd ("Stored Number:3 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 4:
            setCursor (0, 0);
            printlcd ("Stored Number:4 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 5:
            setCursor (0, 0);
            printlcd ("Stored Number:5 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 6:
            setCursor (0, 0);
            printlcd ("Stored Number:6 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 7:
            setCursor (0, 0);
            printlcd ("Stored Number:7 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 8:
            setCursor (0, 0);
            printlcd ("Stored Number:8 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 9:
            setCursor (0, 0);
            printlcd ("Stored Number:9 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 10:
            setCursor (0, 0);
            printlcd ("Stored Float: . ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 11:
            setCursor (0, 0);
            printlcd ("Delete AST IP   ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
    }

}

/****************************************************************
 *Function Name: subMenuSIPPwdStoredStat
 *Description:   Display status of the current SIP password stored
 *               operation
 *
 *Return:        NA
 ***************************************************************/
void subMenuSIPPwdStoredStat (int menu)
{
    switch (menu)
    {
        case 0:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Stored Number:0 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 1:
            setCursor (0, 0);
            printlcd ("Stored Number:1 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 2:
            setCursor (0, 0);
            printlcd ("Stored Number:2 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 3:
            setCursor (0, 0);
            printlcd ("Stored Number:3 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 4:
            setCursor (0, 0);
            printlcd ("Stored Number:4 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 5:
            setCursor (0, 0);
            printlcd ("Stored Number:5 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 6:
            setCursor (0, 0);
            printlcd ("Stored Number:6 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 7:
            setCursor (0, 0);
            printlcd ("Stored Number:7 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 8:
            setCursor (0, 0);
            printlcd ("Stored Number:8 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 9:
            setCursor (0, 0);
            printlcd ("Stored Number:9 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 10:
            setCursor (0, 0);
            printlcd ("Delete SIP PWD  ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
    }
}

/****************************************************************
 *Function Name: subMenuSIPIdStoredStat
 *Description:   Display status of the current SIP ID stored
 *               operation
 *
 *Return:        NA
 ***************************************************************/
void subMenuSIPIdStoredStat (int menu)
{
    switch (menu)
    {
        case 0:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Stored Number:0 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 1:
            setCursor (0, 0);
            printlcd ("Stored Number:1 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 2:
            setCursor (0, 0);
            printlcd ("Stored Number:2 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 3:
            setCursor (0, 0);
            printlcd ("Stored Number:3 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 4:
            setCursor (0, 0);
            printlcd ("Stored Number:4 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 5:
            setCursor (0, 0);
            printlcd ("Stored Number:5 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 6:
            setCursor (0, 0);
            printlcd ("Stored Number:6 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 7:
            setCursor (0, 0);
            printlcd ("Stored Number:7 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 8:
            setCursor (0, 0);
            printlcd ("Stored Number:8 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 9:
            setCursor (0, 0);
            printlcd ("Stored Number:9 ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
        case 10:
            setCursor (0, 0);
            printlcd ("Delete SIP ID   ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);
        break;
    }
}

/****************************************************************
 *Function Name: subMenuSIPAstIp
 *Description:   Character selection menu, for editing purposes.
 *               Also selection to display stored Asterisk
 *               IP address
 *
 *Return:        NA
 ***************************************************************/
void subMenuSIPAstIp (int menu)
{
    char dummyBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    switch (menu)
    {
        // Number 0 + Number 1 - Selection: Select Number:0
        case 0:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:0 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:1 ", 16);
        break;
        // Number 1 + Number 2 - Selection: Select Number:1
        case 1:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:1 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:2 ", 16);
        break;
        // Number 2 + Number 3 - Selection: Select Number:2
        case 2:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:2 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:3 ", 16);
        break;
        // Number 3 + Number 4 - Selection: Select Number:3
        case 3:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:3 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:4 ", 16);
        break;
        // Number 4 + Number 5 - Selection: Select Number:4
        case 4:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:4 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:5 ", 16);
        break;
        // Number 5 + Number 6 - Selection: Select Number:5
        case 5:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:5 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:6 ", 16);
        break;
        // Number 6 + Number 7 - Selection: Select Number:6
        case 6:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:6 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:7 ", 16);
        break;
        // Number 7 + Number 8 - Selection: Select Number:7
        case 7:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:7 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:8 ", 16);
        break;
        // Number 8 + Number 9 - Selection: Select Number:8
        case 8:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:8 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:9 ", 16);
        break;
        // Number 9 + Float - Selection: Select Number:9
        case 9:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:9 ", 16);
            setCursor(0, 1);
            printlcd ("Select Float: . ", 16);
        break;
        // Float + Check AST IP - Selection: Select Float: .
        case 10:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Float: . ", 16);
            setCursor(0, 1);
            printlcd ("1-Check AST IP  ", 16);
        break;
        // Check AST IP + Delete AST IP - Selection: Check AST IP
        case 11:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("1-Check AST IP  ", 16);
            setCursor(0, 1);
            printlcd ("2-Delete AST IP ", 16);
            //printlcd ("2-Exit Editing   ", 16);
        break;
        // Delete AST IP + Exit Editing - Selection: Delete AST IP
        case 12:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("2-Delete AST IP ", 16);
            setCursor(0, 1);
            printlcd ("3-Exit Editing  ", 16);
            //printlcd ("2-Exit Editing   ", 16);
        break;
        // Exit Editing - Selection: Exit Editing
        case 13:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("3-Exit Editing  ", 16);
            setCursor(0, 1);
            printlcd (dummyBuff, 16);
        break;
    }
}

/****************************************************************
 *Function Name: subMenuSIPPswd
 *Description:   Character selection menu, for editing purposes.
 *               Also selection to display stored SIP password
 *
 *Return:        NA
 ***************************************************************/
void subMenuSIPPswd (int menu)
{
    char dummyBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    switch (menu)
    {
        // Number 0 + Number 1 - Selection: Select Number:0
        case 0:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:0 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:1 ", 16);
        break;
        // Number 1 + Number 2 - Selection: Select Number:1
        case 1:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:1 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:2 ", 16);
        break;
        // Number 2 + Number 3 - Selection: Select Number:2
        case 2:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:2 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:3 ", 16);
        break;
        // Number 3 + Number 4 - Selection: Select Number:3
        case 3:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:3 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:4 ", 16);
        break;
        // Number 4 + Number 5 - Selection: Select Number:4
        case 4:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:4 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:5 ", 16);
        break;
        // Number 5 + Number 6 - Selection: Select Number:5
        case 5:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:5 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:6 ", 16);
        break;
        // Number 6 + Number 7 - Selection: Select Number:6
        case 6:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:6 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:7 ", 16);
        break;
        // Number 7 + Number 8 - Selection: Select Number:7
        case 7:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:7 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:8 ", 16);
        break;
        // Number 8 + Number 9 - Selection: Select Number:8
        case 8:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:8 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:9 ", 16);
        break;
        // Number 9 + Check SIP PWD . - Selection: Select Number:9
        case 9:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:9 ", 16);
            setCursor(0, 1);
            printlcd ("1-Check SIP PWD ", 16);
        break;
        // Check SIP PWD + Delete SIP PWD - Selection: Check SIP PWD .
        case 10:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("1-Check SIP PWD ", 16);
            setCursor(0, 1);
            printlcd ("2-Delete SIP PWD", 16);
            //printlcd ("2-Exit Editing   ", 16);
        break;
        // Delete SIP PWD + Exit Editing - Selection: Delete SIP PWD
        case 11:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("2-Delete SIP PWD", 16);
            setCursor(0, 1);
            printlcd ("3-Exit Editing  ", 16);
            //printlcd ("2-Exit Editing   ", 16);
        break;
        // Exit Editing - Selection: Exit Editing
        case 12:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("3-Exit Editing  ", 16);
            setCursor(0, 1);
            printlcd (dummyBuff, 16);
        break;
    }
}

/****************************************************************
 *Function Name: subMenuSIPId
 *Description:   Character selection menu, for editing purposes.
 *               Also selection to display stored SIP ID
 *
 *Return:        NA
 ***************************************************************/
void subMenuSIPId (int menu)
{
    char dummyBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    switch (menu)
    {
        // Number 0 + Number 1 - Selection: Select Number:0
        case 0:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:0 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:1 ", 16);
        break;
        // Number 1 + Number 2 - Selection: Select Number:1
        case 1:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:1 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:2 ", 16);
        break;
        // Number 2 + Number 3 - Selection: Select Number:2
        case 2:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:2 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:3 ", 16);
        break;
        // Number 3 + Number 4 - Selection: Select Number:3
        case 3:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:3 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:4 ", 16);
        break;
        // Number 4 + Number 5 - Selection: Select Number:4
        case 4:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:4 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:5 ", 16);
        break;
        // Number 5 + Number 6 - Selection: Select Number:5
        case 5:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:5 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:6 ", 16);
        break;
        // Number 6 + Number 7 - Selection: Select Number:6
        case 6:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:6 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:7 ", 16);
        break;
        // Number 7 + Number 8 - Selection: Select Number:7
        case 7:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:7 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:8 ", 16);
        break;
        // Number 8 + Number 9 - Selection: Select Number:8
        case 8:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:8 ", 16);
            setCursor(0, 1);
            printlcd ("Select Number:9 ", 16);
        break;
        // Number 9 + Check SIP ID . - Selection: Select Number:9
        case 9:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("Select Number:9 ", 16);
            setCursor(0, 1);
            printlcd ("1-Check SIP ID  ", 16);
        break;
        // Check SIP ID + Exit Editing - Selection: Check SIP ID .
        case 10:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("1-Check SIP ID  ", 16);
            setCursor(0, 1);
            printlcd ("2-Delete SIP ID ", 16);
            //printlcd ("2-Exit Editing   ", 16);
        break;
        // Delete SIP ID + Exit Editing - Selection: Delete SIP ID
        case 11:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("2-Delete SIP ID ", 16);
            setCursor(0, 1);
            printlcd ("3-Exit Editing  ", 16);
            //printlcd ("2-Exit Editing   ", 16);
        break;
        // Exit Editing - Selection: Exit Editing
        case 12:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("3-Exit Editing  ", 16);
            setCursor(0, 1);
            printlcd (dummyBuff, 16);
        break;
    }
}

/****************************************************************
 *Function Name: checkEnblDsblSet
 *Description:   Check and display current enable or disable
 *               setting for PTT, microphone, uudio and
 *               audio multicast
 *
 *Return:        NA
 ***************************************************************/
void checkEnblDsblSet (unsigned char type)
{
    char tempBuff[16];

    // Initialize buffer
    memset(&tempBuff, 0, sizeof(tempBuff));

    // PTT setting
    if (type == 0x01)
    {
        // PTT enabled
        if (pttSetting)
        {
            setCursor (0, 0);
            printlcd ("PTT Setting:    ", 16);
            setCursor(0, 1);
            printlcd ("ENABLED         ", 16);
        }
        // PTT disabled
        else
        {
            setCursor (0, 0);
            printlcd ("PTT Setting:    ", 16);
            setCursor(0, 1);
            printlcd ("DISABLED        ", 16);
        }
    }
    // MIC setting
    else if (type == 0x02)
    {
        // MIC enabled
        if (micSetting)
        {
            setCursor (0, 0);
            printlcd ("MIC Setting:    ", 16);
            setCursor(0, 1);
            printlcd ("ENABLED         ", 16);
        }
        // MIC disabled
        else
        {
            setCursor (0, 0);
            printlcd ("MIC Setting:    ", 16);
            setCursor(0, 1);
            printlcd ("DISABLED        ", 16);
        }
    }
    // AUDIO setting
    else if (type == 0x03)
    {
        // AUDIO enabled
        if (audSetting)
        {
            setCursor (0, 0);
            printlcd ("AUD Setting:    ", 16);
            setCursor(0, 1);
            printlcd ("ENABLED         ", 16);
        }
        // AUDIO disabled
        else
        {
            setCursor (0, 0);
            printlcd ("AUD Setting:    ", 16);
            setCursor(0, 1);
            printlcd ("DISABLED        ", 16);
        }
    }
    // AUDIO multicast setting
    else if (type == 0x04)
    {
        // AUDIO multicast enabled
        if (amuSetting)
        {
            setCursor (0, 0);
            printlcd ("AMU Setting:    ", 16);
            setCursor(0, 1);
            printlcd ("ENABLED         ", 16);
        }
        // AUDIO multicast disabled
        else
        {
            setCursor (0, 0);
            printlcd ("AMU Setting:    ", 16);
            setCursor(0, 1);
            printlcd ("DISABLED        ", 16);
        }
    }
}

/****************************************************************
 *Function Name: subMenuSIPIdStoredStat
 *Description:   Display status of the current SIP ID stored
 *               operation
 *
 *Return:        NA
 ***************************************************************/
void subMenuEnblDsblStoredStat (unsigned char type, unsigned char option)
{
    // PTT setting
    if (type == 0x01)
    {
        // Enabled
        if (option)
        {
            setCursor (0, 0);
            printlcd ("Enable PTT      ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);

            // Stored current PTT setting
            pttSetting = 0x01;
        }
        // Disabled
        else
        {
            setCursor (0, 0);
            printlcd ("Disable PTT      ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);

            // Stored current PTT setting
            pttSetting = 0x00;
        }

    }
    // MIC setting
    else if (type == 0x02)
    {
        // Enabled
        if (option)
        {
            setCursor (0, 0);
            printlcd ("Enable MIC      ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);

            // Stored the current MIC setting
            micSetting = 0x01;
        }
        // Disabled
        else
        {
            setCursor (0, 0);
            printlcd ("Disable MIC     ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);

            // Stored the current MIC setting
            micSetting = 0x00;
        }
    }
    // AUDIO setting
    else if (type == 0x03)
    {
        // Enabled
        if (option)
        {
            setCursor (0, 0);
            printlcd ("Enable AUD      ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);

            // Stored the current AUD setting
            audSetting = 0x01;
        }
        // Disable
        else
        {
            setCursor (0, 0);
            printlcd ("Disable AUD     ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);

            // Stored the current AUD setting
            audSetting = 0x00;
        }
    }
    // AUDIO multicast setting
    else if (type == 0x04)
    {
        // Enabled
        if (option)
        {
            setCursor (0, 0);
            printlcd ("Enable AMU      ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);

            // Stored the current AMU setting
            amuSetting = 0x01;
        }
        // Disabled
        else
        {
            setCursor (0, 0);
            printlcd ("Disable AMU     ", 16);
            setCursor(0, 1);
            printlcd ("Successfull     ", 16);

            // Stored the current AMU setting
            amuSetting = 0x00;
        }
    }
}

/****************************************************************
 *Function Name: subMenuEnblDsbl
 *Description:   Enable and disable  setting sub menu LCD
 *               display selection
 *
 *Return:        NA
 ***************************************************************/
void subMenuEnblDsbl (int menu, unsigned char type)
{
    char dummyBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    switch (menu)
    {
        // Enable. + Disable - Selection: 1-Enable
        case 0:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("1-Enable        ", 16);
            setCursor(0, 1);
            printlcd ("2-Disable       ", 16);
        break;
        // Disable + Current setting - Selection: 2-Disable
        case 1:
            // PTT setting
            if (type == 0x01)
            {
                setCursor (0, 0);
                printlcd ("2-Disable       ", 16);
                setCursor(0, 1);
                printlcd ("3-Check PTT Set.", 16);
            }
            // MIC setting
            else if (type == 0x02)
            {
                setCursor (0, 0);
                printlcd ("2-Disable       ", 16);
                setCursor(0, 1);
                printlcd ("3-Check MIC Set.", 16);
            }
            // AUDIO setting
            else if (type == 0x03)
            {
                setCursor (0, 0);
                printlcd ("2-Disable       ", 16);
                setCursor(0, 1);
                printlcd ("3-Check AUD Set.", 16);
            }
            // AUDIO multicast setting
            else if (type == 0x04)
            {
                setCursor (0, 0);
                printlcd ("2-Disable       ", 16);
                setCursor(0, 1);
                printlcd ("3-Check AMU Set.", 16);
            }
        break;
        // Current setting + Exit Sub Menu - Selection: 3-(Current setting)
        case 2:
            // PTT setting
            if (type == 0x01)
            {
                setCursor (0, 0);
                printlcd ("3-Check PTT Set.", 16);
                setCursor(0, 1);
                printlcd ("4-Exit Editing  ", 16);
            }
            // MIC setting
            else if (type == 0x02)
            {
                setCursor (0, 0);
                printlcd ("3-Check MIC Set.", 16);
                setCursor(0, 1);
                printlcd ("4-Exit Editing  ", 16);
            }
            // AUDIO setting
            else if (type == 0x03)
            {
                setCursor (0, 0);
                printlcd ("3-Check AUD Set.", 16);
                setCursor(0, 1);
                printlcd ("4-Exit Editing  ", 16);
            }
            // AUDIO multicast setting
            else if (type == 0x04)
            {
                setCursor (0, 0);
                printlcd ("3-Check AMU Set.", 16);
                setCursor(0, 1);
                printlcd ("4-Exit Editing  ", 16);
            }
        break;
        // Exit Main Menu - Selection: 5-Exit Main Menu
        case 3:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("4-Exit Editing  ", 16);
            setCursor(0, 1);
            printlcd (dummyBuff, 16);
        break;
    }
}

/****************************************************************
 *Function Name: subMenuSIPCnfg
 *Description:   SIP configuration sub menu LCD display selection
 *
 *Return:        NA
 ***************************************************************/
void subMenuSIPCnfg (int menu)
{
    char dummyBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    switch (menu)
    {
        // SIP ID + SIP Password - Selection: 1-SIP ID
        case 0:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("1-SIP ID        ", 16);
            setCursor(0, 1);
            printlcd ("2-SIP Password  ", 16);
        break;
        // SIP Password + Asterisk IP - Selection: 2-SIP Password
        case 1:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("2-SIP Password  ", 16);
            setCursor(0, 1);
            printlcd ("3-Asterisk IP   ", 16);
        break;
        // Asterisk IP + PTT Setting - Selection: 3-Asterisk IP
        case 2:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("3-Asterisk IP   ", 16);
            setCursor(0, 1);
            printlcd ("4-PTT Setting   ", 16);
        break;
        // PTT Setting + MIC Setting - Selection: 4-PTT Setting
        case 3:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("4-PTT Setting   ", 16);
            setCursor(0, 1);
            printlcd ("5-MIC Setting   ", 16);
        break;
        // MIC Setting + AUDIO Setting - Selection: 5-MIC Setting
        case 4:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("5-MIC Setting   ", 16);
            setCursor(0, 1);
            printlcd ("6-AUDIO Setting ", 16);
        break;
        // AUDIO Setting + A.MULT Setting - Selection: 6-AUDIO Setting
        case 5:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("6-AUDIO Setting ", 16);
            setCursor(0, 1);
            printlcd ("7-A.MULT Setting", 16);
        break;
        // A.MULT Setting + PTT Time Out - Selection: 7-A.MULT Setting
        case 6:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("7-A.MULT Setting", 16);
            setCursor(0, 1);
            printlcd ("8-PTT Time Out  ", 16);
        break;
        // PTT Time Out + PTT Mode - Selection: 8-PTT Time Out
        case 7:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("8-PTT Time Out  ", 16);
            setCursor(0, 1);
            printlcd ("9-PTT Mode      ", 16);
        break;
        // PTT Mode + Exit Settings - Selection: 9-PTT Mode
        case 8:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("9-PTT Mode      ", 16);
            setCursor(0, 1);
            printlcd ("10-Exit Settings", 16);
        break;
        // Exit Settings - Selection: 10-Exit Settings
        case 9:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("10-Exit Settings", 16);
            setCursor(0, 1);
            printlcd (dummyBuff, 16);
        break;
    }
}

/****************************************************************
 *Function Name: menuRIC
 *Description: Get current controller world IP address
 *
 *Return:      0x00-Failed, 0x01-Successfull
 ***************************************************************/
void menuRIC (int menu)
{
    char dummyBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    switch (menu)
    {
        // SIP Config. + VOX Config - Selection: 1-SIP Config.
        case 0:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("1-SIP Config.   ", 16);
            setCursor(0, 1);
            printlcd ("2-VOX Config    ", 16);
        break;
        // VOX Config + Exit Sub Menu - Selection: 2-VOX Config
        case 1:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("2-VOX Config    ", 16);
            setCursor(0, 1);
            printlcd ("3-Exit Sub Menu ", 16);
        break;
        // Exit Main Menu - Selection: 6-Exit Main Menu
        case 2:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("3-Exit Sub Menu ", 16);
            setCursor(0, 1);
            printlcd (dummyBuff, 16);
        break;
    }
}

/****************************************************************
 *Function Name: menuWorldIP
 *Description: Get current controller world IP address
 *
 *Return:      0x00-Failed, 0x01-Successfull
 ***************************************************************/
unsigned char menuWorldIP (void)
{
    FILE *fp;

    int indx = 0;
    unsigned char numbFound = 0x00;
    unsigned char spaceFound = 0x00;
    unsigned char worldFound = 0x00;

    char buffer[1035];
    char ipAddress[16];
    char dummyBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    memset(buffer,0x00,sizeof(buffer));
    memset(ipAddress,0x00,sizeof(ipAddress));

    // Execute command to get controller IP address list
    fp = popen("ifconfig","r");
    if (fp == NULL)
    {
        return 0x00; // Error status
    }
    while (fgets(buffer, sizeof(buffer)-1, fp) != NULL)
    {
        // Search for wwan0 information
        if (!worldFound)
        {
            // Go to this line of information
            if (strstr(buffer, "wwan0") != NULL)
            {
                worldFound = 0x01;
            }
        }
        // Found wwan0
        else
        {
            // Go to this line of information
            if (strstr(buffer, "inet") != NULL)
            {
                // Retrieve controller local IP address - xxx.xxx.xxx.xxx
                for (int i = 0; i < ((int)sizeof(buffer) - 1); i++)
                {
                    // Get only IP address character
                    if ((buffer[i] == '0') || (buffer[i] == '1') || (buffer[i] == '2') || (buffer[i] == '3') || (buffer[i] == '4')
                        || (buffer[i] == '5') || (buffer[i] == '6') || (buffer[i] == '7') || (buffer[i] == '8') || (buffer[i] == '9')
                        || (buffer[i] == '.'))
                    {
                        ipAddress[indx] = buffer[i];
                        indx++;
                        numbFound = 0x01;
                    }
                    // Previously IP address number character already found
                    else if (numbFound)
                    {
                        // Check for empty spaces, indicate IP address are completed
                        if ((buffer[i] == ' ') && (!spaceFound))
                        {
                            spaceFound = 0x01;
                            // Full IP address range, exit loop
                            if (indx == 16)
                            {
                                ipAddress[indx] = ' ';
                                break;
                            }
                            // Short IP address, put empty character inside the buffer
                            else if (indx < 16)
                            {
                                ipAddress[indx] = ' ';
                                indx++;
                            }
                        }
                        // Previously found space, put the rest of IP address buffer with empty character
                        else if (spaceFound)
                        {
                            // Full IP address range, exit loop
                            if (indx == 16)
                            {
                                ipAddress[indx] = ' ';
                                break;
                            }
                            // Short IP address, put empty character inside the buffer
                            else if (indx < 16)
                            {
                                ipAddress[indx] = ' ';
                                indx++;
                            }
                        }
                    }
                }
                // Exit loop
                if (numbFound)
                {
                    break;
                }
            }
        }
    }
    // Close the file if still open.
    if(fp)
    {
        fclose(fp);
    }
    clear(); // Clear first LCD previous contents
    setCursor (0, 0);
    printlcd ("IP Address:     ", 16);
    setCursor(0, 1);
    printlcd (ipAddress, 16);

    return 0x01;
}

/****************************************************************
 *Function Name: menuLocalIP
 *Description: Get current controller local IP address
 *
 *Return:      0x00-Failed, 0x01-Successfull
 ***************************************************************/
unsigned char menuLocalIP (void)
{
    FILE *fp;

    int indx = 0;
    unsigned char numbFound = 0x00;
    unsigned char spaceFound = 0x00;

    char buffer[1035];
    char ipAddress[16];
    char dummyBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    memset(buffer,0x00,sizeof(buffer));
    memset(ipAddress,0x00,sizeof(ipAddress));

    // Execute command to get controller IP address list
    fp = popen("ifconfig","r");
    if (fp == NULL)
    {
        return 0x00; // Error status
    }
    while (fgets(buffer, sizeof(buffer)-1, fp) != NULL)
    {
        // Go to this line of information
        if (strstr(buffer, "inet") != NULL)
        {
            // Retrieve controller local IP address - xxx.xxx.xxx.xxx
            for (int i = 0; i < ((int)sizeof(buffer) - 1); i++)
            {
                // Get only IP address character
                if ((buffer[i] == '0') || (buffer[i] == '1') || (buffer[i] == '2') || (buffer[i] == '3') || (buffer[i] == '4')
                    || (buffer[i] == '5') || (buffer[i] == '6') || (buffer[i] == '7') || (buffer[i] == '8') || (buffer[i] == '9')
                    || (buffer[i] == '.'))
                {
                    ipAddress[indx] = buffer[i];
                    indx++;
                    numbFound = 0x01;
                }
                // Previously IP address number character already found
                else if (numbFound)
                {
                    // Check for empty spaces, indicate IP address are completed
                    if ((buffer[i] == ' ') && (!spaceFound))
                    {
                        spaceFound = 0x01;
                        // Full IP address range, exit loop
                        if (indx == 16)
                        {
                            ipAddress[indx] = ' ';
                            break;
                        }
                        // Short IP address, put empty character inside the buffer
                        else if (indx < 16)
                        {
                            ipAddress[indx] = ' ';
                            indx++;
                        }
                    }
                    // Previously found space, put the rest of IP address buffer with empty character
                    else if (spaceFound)
                    {
                        // Full IP address range, exit loop
                        if (indx == 16)
                        {
                            ipAddress[indx] = ' ';
                            break;
                        }
                        // Short IP address, put empty character inside the buffer
                        else if (indx < 16)
                        {
                            ipAddress[indx] = ' ';
                            indx++;
                        }
                    }
                }
            }
            // Exit loop
            if (numbFound)
            {
                break;
            }
        }
    }
    // Close the file if still open.
    if(fp)
    {
        fclose(fp);
    }
    clear(); // Clear first LCD previous contents
    setCursor (0, 0);
    printlcd ("IP Address:     ", 16);
    setCursor(0, 1);
    printlcd (ipAddress, 16);

    return 0x01;
}

/****************************************************************
 *Function Name: menuTemperature
 *Description: Get current controller temperature
 *
 *Return:      0x00-Failed, 0x01-Successfull
 ***************************************************************/
unsigned char menuTemperature (void)
{
    FILE *fp;

    int a = 0;
    unsigned char plusfound = 0x00;

    char buffer[1035];
    char temperature[9];
    char dummyBuff[16];
    char temperatureBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    memset(buffer,0x00,sizeof(buffer));
    memset(temperature,0x00,sizeof(temperature));
    memset(temperatureBuff,0x00,sizeof(temperatureBuff));

    // Execute command to get controller current temperature
    fp = popen("sensors","r");
    if (fp == NULL)
    {
        return 0x00; // Error status
    }
    while (fgets(buffer, sizeof(buffer)-1, fp) != NULL)
    {
        //if (strstr(buffer, "Physical id 0") != NULL)
        if (strstr(buffer, "Package id 0") != NULL)
        {
            for (int i = 0; i < ((int)sizeof(buffer) - 1); i++)
            {
                // Only accepts below character
                if ((buffer[i] == '0') || (buffer[i] == '1') || (buffer[i] == '2') || (buffer[i] == '3') || (buffer[i] == '4')
                    || (buffer[i] == '5') || (buffer[i] == '6') || (buffer[i] == '7') || (buffer[i] == '8') || (buffer[i] == '9')
                    || (buffer[i] == '+') || (buffer[i] == '.') || (buffer[i] == ' '))
                //if (buffer[i] != 'C')
                {
                    if (buffer[i] == '+')
                    {
                        temperature[a] = buffer[i];
                        a++;
                        plusfound = 0x01;
                    }
                    else if (plusfound == 0x01)
                    {
                        temperature[a] = buffer[i];
                        a++;
                    }
                }
                // Found extended ASCII character, exit loop
                else if ((buffer[i] != '0') || (buffer[i] != '1') || (buffer[i] != '2') || (buffer[i] != '3') || (buffer[i] != '4')
                    || (buffer[i] != '5') || (buffer[i] != '6') || (buffer[i] != '7') || (buffer[i] != '8') || (buffer[i] != '9')
                    || (buffer[i] != '+') || (buffer[i] != '.') || (buffer[i] != ' '))
                {
                    if (plusfound == 0x01)
                    {
                        temperature[a] = '\0';
                        break;
                    }
                }
            }
            if (plusfound == 0x01)
            {
                break;
            }
        }
    }
    // Close the file if still open.
    if(fp)
    {
        fclose(fp);
    }
    // Go through temperature buffer contents
    for (int i = 0; i < 9; i++)
    {
        if (temperature != '\0')
        {
            temperatureBuff[i] = temperature[i];
        }
        else if (temperature == '\0')
        {
            break;
        }
    }
    // Put a empty character to the rest of the temperature LCD display buffer
    for (int i = 0; i < 16; i++)
    {
        // Insert the empty character
        if (temperatureBuff[i] == 0x00)
        {
            temperatureBuff[i] = ' ';
        }
    }
    clear(); // Clear first LCD previous contents
    setCursor (0, 0);
    printlcd ("Temperature[C]: ", 16);
    setCursor(0, 1);
    printlcd (temperatureBuff, 16);

    return 0x01;
}

/****************************************************************
 *Function Name: menuLCD
 *Description:   Display main menu selection on the LCD
 *
 *Return:        NA
 ***************************************************************/
void menuLCD (int menu)
{
    char dummyBuff[16];

    dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

    switch (menu)
    {
        // Local IP + World IP - Selection: 1-Local IP
        case 0:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("1-Local IP      ", 16);
            setCursor(0, 1);
            printlcd ("2-World IP      ", 16);
        break;
        // World IP address: + Temperature - Selection: 2-World IP
        case 1:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("2-World IP      ", 16);
            setCursor(0, 1);
            printlcd ("3-Temperature   ", 16);
        break;
        // Temperature + RIC Settings - Selection: 3-Temperature
        case 2:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("3-Temperature   ", 16);
            setCursor(0, 1);
            printlcd ("4-RIC Settings  ", 16);
        break;
        // RIC Settings + VOIP Ext. List - Selection: 4-RIC Settings
        case 3:
            //lear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("4-RIC Settings  ", 16);
            setCursor(0, 1);
            printlcd ("5-VOIP Ext. List", 16);
        break;
        // VOIP Ext. List + Exit Main Menu - Selection: 5-VOIP Ext. List
        case 4:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("5-VOIP Ext. List", 16);
            setCursor(0, 1);
            printlcd ("6-Exit Main Menu", 16);
        break;
        // Exit Main Menu - Selection: 6-Exit Main Menu
        case 5:
            //clear(); // Clear first LCD previous contents
            setCursor (0, 0);
            printlcd ("6-Exit Main Menu", 16);
            setCursor(0, 1);
            printlcd (dummyBuff, 16);
        break;
    }
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

    char *pCurrDates;
	char *pCurrTimes;

	unsigned char upPressedOnce = 0x00;
	unsigned char dwnPressedOnce = 0x00;
	unsigned char retStoredSipId = 0x00;
    unsigned char retStoredSipPwd = 0x00;
    unsigned char retStoredSipIpAddr = 0x00;

	int temperatureMenu = 0;
	int locIpAddrMenu = 0;
	int wrldIpAddrMenu = 0;
    int initStartMenu = 0;
    int firstMenuCnt = 0;
    int ricMenu = 0;
    int ricMenuIdx = 0;
    int ricSubMenuSIP = 0;
    int ricSubMenuSIPIdx = 0;
    int initRicMenu = 0;
    int ricMenuCnt = 0;
    int sipIdEdit = 0;
    int sipIdIdx = 0;
    int saveSipId = 0;
    int chkSipId = 0;
    int delSipId = 0;
    int sipPwdEdit = 0;
    int sipPwdIdx = 0;
    int savePwdId = 0;
    int chkSipPwd = 0;
    int delSipPwd = 0;
    int sipIpAddrEdit = 0;
    int sipIpAddrIdx = 0;
    int saveIpAddr = 0;
    int chkIpAddr = 0;
    int delIpAddr = 0;
    int sipPttSetEdit = 0;
    int sipPttSetIdx = 0;
    int saveEnblDsbl = 0;
    int chkPttSet = 0;
    int sipMicSetEdit = 0;
    int sipMicSetIdx = 0;
    int chkMicSet = 0;
    int sipAudSetEdit = 0;
    int sipAudSetIdx = 0;
    int chkAudSet = 0;
    int sipAudMSetEdit = 0;
    int sipAudMSetIdx = 0;
    int chkAudMSet = 0;

	// Thread main loop
    while(1)
    {
        //pthread_mutex_lock(&lcdMenuMutex);

        pCurrDates = get_curr_date(); // Get current date
        pCurrTimes = get_curr_time(); // Get current time

        //pthread_mutex_unlock(&lcdMenuMutex);
        // Main menu mode
        if ((mainMenuMode) && (!exitMainMenu))
        {
            // Initialize start menu - Give a 5s delay to prevent process goes to first menu
            if (initStartMenu == 0)
            {
                firstMenuCnt++;
                // 1s delay elapsed, start the full main menu process
                if (firstMenuCnt == 10)
                {
                    initStartMenu = 1;
                    firstMenuCnt = 0;
                }
            }
            if (initRicMenu == 0)
            {
                ricMenuCnt++;
                if (ricMenuCnt == 10)
                {
                    initRicMenu = 1;
                    ricMenuCnt = 0;
                }
            }
            // Check whether main menu exit are selected
            if (mainMenuIdx == 5)
            {
                // Exit main menu are selected
                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                {
                    exitMainMenu = 0x01; // Set exit main menu flag
                }
            }
            // Get controller local IP address
            // Each time after 5s elapsed - Only applicable to first main menu, to prevent a loop selection
            else if ((mainMenuIdx == 0) && (initStartMenu == 1))
            {
                // Check UP and DOWN press for controller local IP address sub info
                if (locIpAddrMenu == 0)
                {
                    // First GPIO scan
                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                    {
                        // Delay 0.1s
                        msleep(100);
                        // Entering local IP address menu
                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                        {
                            menuLocalIP();
                            locIpAddrMenu = 1;
                        }
                    }
                }
                // Check UP and DOWN release state
                else if (locIpAddrMenu == 1)
                {
                    // UP and DOWN confirm in release state, set flag to prepared exit local IP address sub info mode
                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                    {
                        locIpAddrMenu = 2;
                    }
                }
                // Start check UP and DOWN press to exit local IP address sub info mode
                else if (locIpAddrMenu == 2)
                {
                    // First GPIO scan
                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                    {
                        // Delay 0.1s
                        msleep(100);
                        // Exit local IP address menu selection
                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                        {
                            mainMenuIdx = 0;
                            initStartMenu = 0;

                            clear(); // Clear first LCD previous contents
                            menuLCD(mainMenuIdx);

                            locIpAddrMenu = 0;
                        }
                    }
                }
            }
            // Get controller world IP address
            else if (mainMenuIdx == 1)
            {
                // Check UP and DOWN press for controller world IP address sub info
                if (wrldIpAddrMenu == 0)
                {
                    // First GPIO scan
                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                    {
                        // Delay 0.1s
                        msleep(100);
                        // Entering world IP address menu
                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                        {
                            menuWorldIP();
                            wrldIpAddrMenu = 1;
                        }
                    }
                }
                // Check UP and DOWN release state
                else if (wrldIpAddrMenu == 1)
                {
                    // UP and DOWN confirm in release state, set flag to prepared exit world IP address sub info mode
                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                    {
                        wrldIpAddrMenu = 2;
                    }
                }
                // Start check UP and DOWN press to exit world IP address sub info mode
                else if (wrldIpAddrMenu == 2)
                {
                    // First GPIO scan
                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                    {
                        // Delay 0.1s
                        msleep(100);
                        // Exit world IP address menu selection
                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                        {
                            mainMenuIdx = 0;
                            initStartMenu = 0;

                            clear(); // Clear first LCD previous contents
                            menuLCD(mainMenuIdx);

                            wrldIpAddrMenu = 0;
                        }
                    }
                }
            }
            // Get controller temperature info
            else if (mainMenuIdx == 2)
            {
                // Check UP and DOWN press for controller temperature sub info
                if (temperatureMenu == 0)
                {
                    // First GPIO scan
                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                    {
                        // Delay 0.1s
                        msleep(100);
                        // Entering temperature menu
                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                        {
                            menuTemperature();
                            temperatureMenu = 1;
                        }
                    }
                }
                // Check UP and DOWN release state
                else if (temperatureMenu == 1)
                {
                    // UP and DOWN confirm in release state, set flag to prepared exit temperature sub info mode
                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                    {
                        temperatureMenu = 2;
                    }
                }
                // Start check UP and DOWN press to exit temperature sub info mode
                else if (temperatureMenu == 2)
                {
                    // First GPIO scan
                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                    {
                        // Delay 0.1s
                        msleep(100);
                        // Exit temperature menu selection
                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                        {
                            mainMenuIdx = 0;
                            initStartMenu = 0;

                            clear(); // Clear first LCD previous contents
                            menuLCD(mainMenuIdx);

                            temperatureMenu = 0;
                        }
                    }
                }
            }
            // RIC settings - Upon selection, display RIC settings main menu
            else if (mainMenuIdx == 3)
            {
                // Check UP and DOWN press for RIC settings main menu
                if (ricMenu == 0)
                {
                    // First GPIO scan
                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                    {
                        // Delay 0.1s
                        msleep(100);
                        // Entering RIC settings menu
                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                        {
                            ricMenuIdx = 0;
                            menuRIC(ricMenuIdx);
                            ricMenu = 1;
                        }
                    }
                }
                // Check UP and DOWN release state
                else if (ricMenu == 1)
                {
                    // UP and DOWN confirm in release state, set flag to prepared exit RIC settings sub menu mode
                    // Or to check next sub menu option
                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                    {
                        ricMenu = 2;
                    }
                }
                // Start check UP and DOWN press action
                // Scrolling RIC main menu
                else if (ricMenu == 2)
                {
                    // Exit RIC main menu selection
                    if (ricMenuIdx == 2)
                    {
                        // First GPIO scan
                        if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                        {
                            // Delay 0.1s
                            msleep(100);
                            // Exit RIC settings main menu - Confirm selection
                            if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                            {
                                ricMenuIdx = 0;
                                mainMenuIdx = 0;
                                initStartMenu = 0;
                                initRicMenu = 0;
                                upPressedOnce = 0x00;
                                dwnPressedOnce = 0x00;

                                clear(); // Clear first LCD previous contents
                                menuLCD(mainMenuIdx);

                                ricMenu = 0;
                            }
                        }
                    }
                    // SIP configuration main menu selection
                    // 1s delay to eliminate selection loop
                    else if ((ricMenuIdx == 0) && (initRicMenu == 1))
                    {
                        // Check UP and DOWN press for controller SIP configuration main menu
                        if (ricSubMenuSIP == 0)
                        {
                            // First GPIO scan
                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                            {
                                // Delay 0.1s
                                msleep(100);
                                // Entering SIP configuration main menu
                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                {
                                    ricSubMenuSIPIdx = 0;
                                    subMenuSIPCnfg(ricSubMenuSIPIdx);
                                    ricSubMenuSIP = 1;
                                    initRicMenu = 0;
                                }
                            }
                        }
                        // Check UP and DOWN release state
                        else if (ricSubMenuSIP == 1)
                        {
                            // UP and DOWN confirm in release state, set flag to prepared exit SIP configuration sub menu mode
                            // Or to check next sub menu option
                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                            {
                                ricSubMenuSIP = 2;
                            }
                        }
                        // Start check UP and DOWN press action
                        // Scrolling SIP configuration main menu
                        else if (ricSubMenuSIP == 2)
                        {
                            // Exit SIP configuration main menu selection
                            if (ricSubMenuSIPIdx == 9)
                            {
                                // First GPIO scan
                                if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                {
                                    // Delay 0.1s
                                    msleep(100);
                                    // Exit SIP configuration main menu - Confirm selection
                                    if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                    {
                                        ricSubMenuSIPIdx = 0;
                                        ricMenuIdx = 0;
                                        menuRIC(ricMenuIdx);

                                        initRicMenu = 0;
                                        ricSubMenuSIP = 0;
                                    }
                                }
                            }
                            // Entering SIP ID entering selection
                            // 1s delay to eliminate selection loop
                            else if ((ricSubMenuSIPIdx == 0) && (initRicMenu == 1))
                            {
                                // Check UP and DOWN press for controller SIP ID entering selection - 4 digits SIP ID
                                if (sipIdEdit == 0)
                                {
                                    // First GPIO scan
                                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                    {
                                        // Delay 0.1s
                                        msleep(100);
                                        // Entering SIP ID entering selection
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            sipIdIdx = 0;
                                            subMenuSIPId(sipIdIdx);
                                            sipIdEdit = 1;
                                            initRicMenu = 0;
                                        }
                                    }
                                }
                                // Check UP and DOWN release state
                                else if (sipIdEdit == 1)
                                {
                                    // UP and DOWN confirm in release state, set flag to prepared exit SIP ID entering selection
                                    // Or to check next sub menu option
                                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                    {
                                        sipIdEdit = 2;
                                    }
                                }
                                // Start check UP and DOWN press action
                                // Scrolling SIP ID editing entering selection
                                else if (sipIdEdit == 2)
                                {
                                    // Exit Editing - Selection
                                    if (sipIdIdx == 12)
                                    {
                                        // First GPIO scan
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            // Delay 0.1s
                                            msleep(100);
                                            // Exit SIP ID entering selection - Confirm selection
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                sipIdIdx = 0;
                                                ricSubMenuSIPIdx = 0;
                                                subMenuSIPCnfg(ricSubMenuSIPIdx);

                                                initRicMenu = 0;
                                                sipIdEdit = 0;
                                            }
                                        }
                                    }
                                    // Select Number:0 - 9
                                    else if ((sipIdIdx == 0) || (sipIdIdx == 1) || (sipIdIdx == 2) || (sipIdIdx == 3) || (sipIdIdx == 4)
                                        || (sipIdIdx == 5) || (sipIdIdx == 6) || (sipIdIdx == 7) || (sipIdIdx == 8) || (sipIdIdx == 9))
                                    {
                                        // Check UP and DOWN press for saving SIP Id process
                                        if (saveSipId == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Saves number 0 - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Stored SIP Id to the buffer
                                                    retStoredSipId = storedSIPId(sipIdIdx);
                                                    // Display successfull stored to the LCD display
                                                    if (retStoredSipId == 0x00)
                                                    {
                                                        subMenuSIPIdStoredStat(sipIdIdx);
                                                        sipIdStoredCnt++;

                                                        saveSipId = 1;
                                                    }
                                                    // Stored SIP Id process completed
                                                    else if ((retStoredSipId == 0x01) || (retStoredSipId == 0x02))
                                                    {
                                                        saveSipId = 1;
                                                    }
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (saveSipId == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                saveSipId = 2;
                                            }
                                        }
                                        // Going back to SIP Id entering selection after 3s
                                        else if (saveSipId == 2)
                                        {
                                            // 3S delay
                                            sleep(3);
                                            sipIdIdx = 0;
                                            subMenuSIPId(sipIdIdx);

                                            initRicMenu = 0;
                                            saveSipId = 0;
                                        }
                                    }
                                    // Check SIP ID
                                    else if (sipIdIdx == 10)
                                    {
                                        // Check UP and DOWN press for checking current stored SIP Id
                                        if (chkSipId == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Check SIP ID - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Get current stored SIP ID
                                                    storedSIPId(10);
                                                    chkSipId = 1;
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (chkSipId == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                chkSipId = 2;
                                            }
                                        }
                                        // Start check UP and DOWN press action
                                        // Exit check stored SIP Id
                                        else if (chkSipId == 2)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Exit check stored SIP Id - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    sipIdIdx = 0;
                                                    subMenuSIPId(sipIdIdx);

                                                    initRicMenu = 0;
                                                    chkSipId = 0;
                                                }
                                            }
                                        }
                                    }
                                    // Delete SIP Id
                                    else if (sipIdIdx == 11)
                                    {
                                        // Check UP and DOWN press for delete current stored SIP Id
                                        if (delSipId == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Delete SIP ID - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Delete local SIP Id buffer and counter
                                                    memset(&sipIdStored, 0, sizeof(sipIdStored));
                                                    sipIdStoredCnt = 0;

                                                    subMenuSIPIdStoredStat(10);

                                                    delSipId = 1;
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (delSipId == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                delSipId = 2;
                                            }
                                        }
                                        // Going back to SIP Id entering selection after 3s
                                        else if (delSipId == 2)
                                        {
                                            // 3S delay
                                            sleep(3);
                                            sipIdIdx = 0;
                                            subMenuSIPId(sipIdIdx);

                                            initRicMenu = 0;
                                            delSipId = 0;
                                        }
                                    }
                                }
                            }
                            // Entering SIP password entering selection
                            // 1s delay to eliminate selection loop
                            else if ((ricSubMenuSIPIdx == 1) && (initRicMenu == 1))
                            {
                                // Check UP and DOWN press for controller SIP password entering selection - 4 digits SIP ID
                                if (sipPwdEdit == 0)
                                {
                                    // First GPIO scan
                                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                    {
                                        // Delay 0.1s
                                        msleep(100);
                                        // Entering SIP Asterisk IP address entering selection
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            sipPwdIdx = 0;
                                            subMenuSIPPswd(sipPwdIdx);
                                            sipPwdEdit = 1;
                                            initRicMenu = 0;
                                        }
                                    }
                                }
                                // Check UP and DOWN release state
                                else if (sipPwdEdit == 1)
                                {
                                    // UP and DOWN confirm in release state, set flag to prepared exit SIP password entering selection
                                    // Or to check next sub menu option
                                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                    {
                                        sipPwdEdit = 2;
                                    }
                                }
                                // Start check UP and DOWN press action
                                // Scrolling SIP password editing entering selection
                                else if (sipPwdEdit == 2)
                                {
                                    // Exit Editing - Selection
                                    if (sipPwdIdx == 12)
                                    {
                                        // First GPIO scan
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            // Delay 0.1s
                                            msleep(100);
                                            // Exit SIP password entering selection - Confirm selection
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                sipPwdIdx = 0;
                                                ricSubMenuSIPIdx = 0;
                                                subMenuSIPCnfg(ricSubMenuSIPIdx);

                                                initRicMenu = 0;
                                                sipPwdEdit = 0;
                                            }
                                        }
                                    }
                                    // Select Number:0 - 9
                                    else if ((sipPwdIdx == 0) || (sipPwdIdx == 1) || (sipPwdIdx == 2) || (sipPwdIdx == 3) || (sipPwdIdx == 4)
                                        || (sipPwdIdx == 5) || (sipPwdIdx == 6) || (sipPwdIdx == 7) || (sipPwdIdx == 8) || (sipPwdIdx == 9))
                                    {
                                        // Check UP and DOWN press for saving SIP password process
                                        if (savePwdId == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Saves number 0 - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Stored SIP password to the buffer
                                                    retStoredSipPwd = storedSIPPwd(sipPwdIdx);
                                                    // Display successfull stored to the LCD display
                                                    if (retStoredSipPwd == 0x00)
                                                    {
                                                        subMenuSIPPwdStoredStat(sipPwdIdx);
                                                        sipPwdStoredCnt++;

                                                        savePwdId = 1;
                                                    }
                                                    // Stored SIP password process completed
                                                    else if ((retStoredSipPwd == 0x01) || (retStoredSipPwd == 0x02))
                                                    {
                                                        savePwdId = 1;
                                                    }
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (savePwdId == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                savePwdId = 2;
                                            }
                                        }
                                        // Going back to SIP password entering selection after 3s
                                        else if (savePwdId == 2)
                                        {
                                            // 3S delay
                                            sleep(3);
                                            sipPwdIdx = 0;
                                            subMenuSIPPswd(sipPwdIdx);

                                            initRicMenu = 0;
                                            savePwdId = 0;
                                        }
                                    }
                                    // Check SIP password
                                    else if (sipPwdIdx == 10)
                                    {
                                        // Check UP and DOWN press for checking current stored SIP password
                                        if (chkSipPwd == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Check SIP password - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    storedSIPPwd(10);
                                                    chkSipPwd = 1;
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (chkSipPwd == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                chkSipPwd = 2;
                                            }
                                        }
                                        // Start check UP and DOWN press action
                                        // Exit check stored SIP Ipassword
                                        else if (chkSipPwd == 2)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Exit check stored SIP password - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    sipPwdIdx = 0;
                                                    subMenuSIPPswd(sipPwdIdx);

                                                    initRicMenu = 0;
                                                    chkSipPwd = 0;
                                                }
                                            }
                                        }
                                    }
                                    // Delete SIP password
                                    else if (sipPwdIdx == 11)
                                    {
                                        // Check UP and DOWN press for delete current stored SIP password
                                        if (delSipPwd == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Delete SIP password - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Delete local SIP password buffer and counter
                                                    memset(&sipPwdStored, 0, sizeof(sipPwdStored));
                                                    sipPwdStoredCnt = 0;

                                                    subMenuSIPPwdStoredStat(10);

                                                    delSipPwd = 1;
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (delSipPwd == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                delSipPwd = 2;
                                            }
                                        }
                                        // Going back to SIP password entering selection after 3s
                                        else if (delSipPwd == 2)
                                        {
                                            // 3S delay
                                            sleep(3);
                                            sipPwdIdx = 0;
                                            subMenuSIPPswd(sipPwdIdx);

                                            initRicMenu = 0;
                                            delSipPwd = 0;
                                        }
                                    }
                                }
                            }
                            // Entering Asterisk IP address entering selection
                            // 1s delay to eliminate selection loop
                            else if ((ricSubMenuSIPIdx == 2) && (initRicMenu == 1))
                            {
                                // Check UP and DOWN press for controller Asterisk IP address entering selection
                                if (sipIpAddrEdit == 0)
                                {
                                    // First GPIO scan
                                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                    {
                                        // Delay 0.1s
                                        msleep(100);
                                        // Entering Asterisk IP address entering selection
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            sipIpAddrIdx = 0;
                                            subMenuSIPAstIp(sipIpAddrIdx);
                                            sipIpAddrEdit = 1;
                                            initRicMenu = 0;
                                        }
                                    }
                                }
                                // Check UP and DOWN release state
                                else if (sipIpAddrEdit == 1)
                                {
                                    // UP and DOWN confirm in release state, set flag to prepared exit Asterisk IP address entering selection
                                    // Or to check next sub menu option
                                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                    {
                                        sipIpAddrEdit = 2;
                                    }
                                }
                                // Start check UP and DOWN press action
                                // Scrolling Asterisk IP address editing entering selection
                                else if (sipIpAddrEdit == 2)
                                {
                                    // Exit Editing - Selection
                                    if (sipIpAddrIdx == 13)
                                    {
                                        // First GPIO scan
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            // Delay 0.1s
                                            msleep(100);
                                            // Exit Asterisk IP address entering selection - Confirm selection
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                sipIpAddrIdx = 0;
                                                ricSubMenuSIPIdx = 0;
                                                subMenuSIPCnfg(ricSubMenuSIPIdx);

                                                initRicMenu = 0;
                                                sipIpAddrEdit = 0;
                                            }
                                        }
                                    }
                                    // Select Number:0 - 9 or float character
                                    else if ((sipIpAddrIdx == 0) || (sipIpAddrIdx == 1) || (sipIpAddrIdx == 2) || (sipIpAddrIdx == 3) || (sipIpAddrIdx == 4)
                                        || (sipIpAddrIdx == 5) || (sipIpAddrIdx == 6) || (sipIpAddrIdx == 7) || (sipIpAddrIdx == 8) || (sipIpAddrIdx == 9)
                                        || (sipIpAddrIdx == 10))
                                    {
                                        // Check UP and DOWN press for saving Asterisk IP address process
                                        if (saveIpAddr == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Saves number 0 - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Stored Asterisk IP address to the buffer
                                                    retStoredSipIpAddr = storedSIPIpAddr(sipIpAddrIdx);
                                                    // Display successfull stored to the LCD display
                                                    if (retStoredSipIpAddr == 0x00)
                                                    {
                                                        subMenuSIPAstIpStoredStat(sipIpAddrIdx);
                                                        sipAstIpStoredCnt++;

                                                        saveIpAddr = 1;
                                                    }
                                                    // Stored Asterisk IP address process completed or
                                                    // Wrong format of Asterisk IP address
                                                    else if ((retStoredSipIpAddr == 0x01) || (retStoredSipIpAddr == 0x03)
                                                        || (retStoredSipIpAddr == 0x02))
                                                    {
                                                        saveIpAddr = 1;
                                                    }
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (saveIpAddr == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                saveIpAddr = 2;
                                            }
                                        }
                                        // Going back to SIP password entering selection after 3s
                                        else if (saveIpAddr == 2)
                                        {
                                            // 3S delay
                                            sleep(3);
                                            sipIpAddrIdx = 0;
                                            subMenuSIPAstIp(sipIpAddrIdx);

                                            initRicMenu = 0;
                                            saveIpAddr = 0;
                                        }
                                    }
                                    // Check Asterisk IP address
                                    else if (sipIpAddrIdx == 11)
                                    {
                                        // Check UP and DOWN press for checking current stored Asterisk IP address
                                        if (chkIpAddr == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Check Asterisk IP address - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    storedSIPIpAddr(11);
                                                    chkIpAddr = 1;
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (chkIpAddr == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                chkIpAddr = 2;
                                            }
                                        }
                                        // Start check UP and DOWN press action
                                        // Exit check stored Asterisk IP address
                                        else if (chkIpAddr == 2)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Exit check stored Asterisk IP address - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    sipIpAddrIdx = 0;
                                                    subMenuSIPAstIp(sipIpAddrIdx);

                                                    initRicMenu = 0;
                                                    chkIpAddr = 0;
                                                }
                                            }
                                        }
                                    }
                                    // Delete Asterisk IP address
                                    else if (sipIpAddrIdx == 12)
                                    {
                                        // Check UP and DOWN press for delete current stored Asterisk IP address
                                        if (delIpAddr == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Delete Asterisk IP address - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Delete local Asterisk IP address buffer and counter
                                                    memset(&sipAstIpAddr, 0, sizeof(sipAstIpAddr));
                                                    sipAstIpStoredCnt = 0;

                                                    subMenuSIPAstIpStoredStat(11);

                                                    delIpAddr = 1;
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (delIpAddr == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                delIpAddr = 2;
                                            }
                                        }
                                        // Going back to Asterisk IP address entering selection after 3s
                                        else if (delIpAddr == 2)
                                        {
                                            // 3S delay
                                            sleep(3);
                                            sipIpAddrIdx = 0;
                                            subMenuSIPAstIp(sipIpAddrIdx);

                                            initRicMenu = 0;
                                            delIpAddr = 0;
                                        }
                                    }
                                }
                            }
                            // Entering PTT setting
                            // 1s delay to eliminate selection loop
                            else if ((ricSubMenuSIPIdx == 3) && (initRicMenu == 1))
                            {
                                // Check UP and DOWN press for controller PTT setting entering selection
                                if (sipPttSetEdit == 0)
                                {
                                    // First GPIO scan
                                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                    {
                                        // Delay 0.1s
                                        msleep(100);
                                        // Entering PTT setting entering selection
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            sipPttSetIdx = 0;
                                            subMenuEnblDsbl(sipPttSetIdx, 0x01);
                                            sipPttSetEdit = 1;
                                            initRicMenu = 0;
                                        }
                                    }
                                }
                                // Check UP and DOWN release state
                                else if (sipPttSetEdit == 1)
                                {
                                    // UP and DOWN confirm in release state, set flag to prepared exit PTT setting selection
                                    // Or to check next sub menu option
                                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                    {
                                        sipPttSetEdit = 2;
                                    }
                                }
                                // Start check UP and DOWN press action
                                // Scrolling PTT setting selection
                                else if (sipPttSetEdit == 2)
                                {
                                    // Exit Editing - Selection
                                    if (sipPttSetIdx == 3)
                                    {
                                        // First GPIO scan
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            // Delay 0.1s
                                            msleep(100);
                                            // Exit PTT setting selection - Confirm selection
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                sipPttSetIdx = 0;
                                                ricSubMenuSIPIdx = 0;
                                                subMenuSIPCnfg(ricSubMenuSIPIdx);

                                                initRicMenu = 0;
                                                sipPttSetEdit = 0;
                                            }
                                        }
                                    }
                                    // Enable or disable PTT setting
                                    else if ((sipPttSetIdx == 0) || (sipPttSetIdx == 1))
                                    {
                                        // Check UP and DOWN press for saving PTT setting process
                                        if (saveEnblDsbl == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Saves enable or disable setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Enable
                                                    if (sipPttSetIdx == 0)
                                                    {
                                                        // Save enable or disable setting
                                                        subMenuEnblDsblStoredStat(0x01, 0x01);
                                                        saveEnblDsbl = 1;
                                                    }
                                                    // Disable
                                                    else
                                                    {
                                                        // Save enable or disable setting
                                                        subMenuEnblDsblStoredStat(0x01, 0x00);
                                                        saveEnblDsbl = 1;
                                                    }
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (saveEnblDsbl == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                saveEnblDsbl = 2;
                                            }
                                        }
                                        else if (saveEnblDsbl == 2)
                                        {
                                            // 3S delay
                                            sleep(3);
                                            sipPttSetIdx = 0;
                                            subMenuEnblDsbl(sipPttSetIdx, 0x01);

                                            initRicMenu = 0;
                                            saveEnblDsbl = 0;
                                        }
                                    }
                                    // Check current PTT setting
                                    else if (sipPttSetIdx == 2)
                                    {
                                        // Check UP and DOWN press for checking current stored PTT setting
                                        if (chkPttSet == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Check current PTT setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Check current PTT setting
                                                    checkEnblDsblSet(0x01);
                                                    chkPttSet = 1;
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (chkPttSet == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                chkPttSet = 2;
                                            }
                                        }
                                        // Start check UP and DOWN press action
                                        // Exit check current PTT setting
                                        else if (chkPttSet == 2)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Exit check current PTT setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    sipPttSetIdx = 0;
                                                    subMenuEnblDsbl(sipPttSetIdx, 0x01);

                                                    initRicMenu = 0;
                                                    chkPttSet = 0;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            // Entering MIC setting
                            // 1s delay to eliminate selection loop
                            else if ((ricSubMenuSIPIdx == 4) && (initRicMenu == 1))
                            {
                                // Check UP and DOWN press for controller MIC setting entering selection
                                if (sipMicSetEdit == 0)
                                {
                                    // First GPIO scan
                                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                    {
                                        // Delay 0.1s
                                        msleep(100);
                                        // Entering MIC setting entering selection
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            sipMicSetIdx = 0;
                                            subMenuEnblDsbl(sipMicSetIdx, 0x02);
                                            sipMicSetEdit = 1;
                                            initRicMenu = 0;
                                        }
                                    }
                                }
                                // Check UP and DOWN release state
                                else if (sipMicSetEdit == 1)
                                {
                                    // UP and DOWN confirm in release state, set flag to prepared exit MIC setting selection
                                    // Or to check next sub menu option
                                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                    {
                                        sipMicSetEdit = 2;
                                    }
                                }
                                // Start check UP and DOWN press action
                                // Scrolling MIC setting selection
                                else if (sipMicSetEdit == 2)
                                {
                                    // Exit Editing - Selection
                                    if (sipMicSetIdx == 3)
                                    {
                                        // First GPIO scan
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            // Delay 0.1s
                                            msleep(100);
                                            // Exit PTT setting selection - Confirm selection
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                sipMicSetIdx = 0;
                                                ricSubMenuSIPIdx = 0;
                                                subMenuSIPCnfg(ricSubMenuSIPIdx);

                                                initRicMenu = 0;
                                                sipMicSetEdit = 0;
                                            }

                                        }
                                    }
                                    // Enable or disable MIC setting
                                    else if ((sipMicSetIdx == 0) || (sipMicSetIdx == 1))
                                    {
                                        // Check UP and DOWN press for saving PTT setting process
                                        if (saveEnblDsbl == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Saves enable or disable setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Enable
                                                    if (sipMicSetIdx == 0)
                                                    {
                                                        // Save enable or disable setting
                                                        subMenuEnblDsblStoredStat(0x02, 0x01);
                                                        saveEnblDsbl = 1;
                                                    }
                                                    // Disable
                                                    else
                                                    {
                                                        // Save enable or disable setting
                                                        subMenuEnblDsblStoredStat(0x02, 0x00);
                                                        saveEnblDsbl = 1;
                                                    }
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (saveEnblDsbl == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                saveEnblDsbl = 2;
                                            }
                                        }
                                        else if (saveEnblDsbl == 2)
                                        {
                                            // 3S delay
                                            sleep(3);
                                            sipMicSetIdx = 0;
                                            subMenuEnblDsbl(sipMicSetIdx, 0x02);

                                            initRicMenu = 0;
                                            saveEnblDsbl = 0;
                                        }
                                    }
                                    // Check current MIC setting
                                    else if (sipMicSetIdx == 2)
                                    {
                                        // Check UP and DOWN press for checking current stored MIC setting
                                        if (chkMicSet == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Check current PTT setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Check current MIC setting
                                                    checkEnblDsblSet(0x02);
                                                    chkMicSet = 1;
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (chkMicSet == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                chkMicSet = 2;
                                            }
                                        }
                                        // Start check UP and DOWN press action
                                        // Exit check current MIC setting
                                        else if (chkMicSet == 2)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Exit check current MIC setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    sipMicSetIdx = 0;
                                                    subMenuEnblDsbl(sipMicSetIdx, 0x02);

                                                    initRicMenu = 0;
                                                    chkMicSet = 0;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            // Entering AUDIO setting
                            // 1s delay to eliminate selection loop
                            else if ((ricSubMenuSIPIdx == 5) && (initRicMenu == 1))
                            {
                                // Check UP and DOWN press for controller AUDIO setting entering selection
                                if (sipAudSetEdit == 0)
                                {
                                    // First GPIO scan
                                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                    {
                                        // Delay 0.1s
                                        msleep(100);
                                        // Entering MIC setting entering selection
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            sipAudSetIdx = 0;
                                            subMenuEnblDsbl(sipAudSetIdx, 0x03);
                                            sipAudSetEdit = 1;
                                            initRicMenu = 0;
                                        }
                                    }
                                }
                                // Check UP and DOWN release state
                                else if (sipAudSetEdit == 1)
                                {
                                    // UP and DOWN confirm in release state, set flag to prepared exit AUDIO setting selection
                                    // Or to check next sub menu option
                                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                    {
                                        sipAudSetEdit = 2;
                                    }
                                }
                                // Start check UP and DOWN press action
                                // Scrolling AUDIO setting selection
                                else if (sipAudSetEdit == 2)
                                {
                                    // Exit Editing - Selection
                                    if (sipAudSetIdx == 3)
                                    {
                                        // First GPIO scan
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            // Delay 0.1s
                                            msleep(100);
                                            // Exit AUDIO setting selection - Confirm selection
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                sipAudSetIdx = 0;
                                                ricSubMenuSIPIdx = 0;
                                                subMenuSIPCnfg(ricSubMenuSIPIdx);

                                                initRicMenu = 0;
                                                sipAudSetEdit = 0;
                                            }
                                        }
                                    }
                                    // Enable or disable AUDIO setting
                                    else if ((sipAudSetIdx == 0) || (sipAudSetIdx == 1))
                                    {
                                        // Check UP and DOWN press for saving AUDIO setting process
                                        if (saveEnblDsbl == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Saves enable or disable setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Enable
                                                    if (sipAudSetIdx == 0)
                                                    {
                                                        // Save enable or disable setting
                                                        subMenuEnblDsblStoredStat(0x03, 0x01);
                                                        saveEnblDsbl = 1;
                                                    }
                                                    // Disable
                                                    else
                                                    {
                                                        // Save enable or disable setting
                                                        subMenuEnblDsblStoredStat(0x03, 0x00);
                                                        saveEnblDsbl = 1;
                                                    }
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (saveEnblDsbl == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                saveEnblDsbl = 2;
                                            }
                                        }
                                        else if (saveEnblDsbl == 2)
                                        {
                                            // 3S delay
                                            sleep(3);
                                            sipAudSetIdx = 0;
                                            subMenuEnblDsbl(sipAudSetIdx, 0x03);

                                            initRicMenu = 0;
                                            saveEnblDsbl = 0;
                                        }
                                    }
                                    // Check current AUDIO setting
                                    else if (sipAudSetIdx == 2)
                                    {
                                        // Check UP and DOWN press for checking current stored AUDIO setting
                                        if (chkAudSet == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Check current PTT setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Check current AUDIO setting
                                                    checkEnblDsblSet(0x03);
                                                    chkAudSet = 1;
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (chkAudSet == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                chkAudSet = 2;
                                            }
                                        }
                                        // Start check UP and DOWN press action
                                        // Exit check current AUDIO setting
                                        else if (chkAudSet == 2)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Exit check current AUDIO setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    sipAudSetIdx = 0;
                                                    subMenuEnblDsbl(sipAudSetIdx, 0x03);

                                                    initRicMenu = 0;
                                                    chkAudSet = 0;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            // Entering AUDIO multicast setting
                            // 1s delay to eliminate selection loop
                            else if ((ricSubMenuSIPIdx == 6) && (initRicMenu == 1))
                            {
                                // Check UP and DOWN press for controller AUDIO multicast setting entering selection
                                if (sipAudMSetEdit == 0)
                                {
                                    // First GPIO scan
                                    if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                    {
                                        // Delay 0.1s
                                        msleep(100);
                                        // Entering MIC setting entering selection
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            sipAudMSetIdx = 0;
                                            subMenuEnblDsbl(sipAudMSetIdx, 0x04);
                                            sipAudMSetEdit = 1;
                                            initRicMenu = 0;
                                        }
                                    }
                                }
                                // Check UP and DOWN release state
                                else if (sipAudMSetEdit == 1)
                                {
                                    // UP and DOWN confirm in release state, set flag to prepared exit AUDIO multicast setting selection
                                    // Or to check next sub menu option
                                    if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                    {
                                        sipAudMSetEdit = 2;
                                    }
                                }
                                // Start check UP and DOWN press action
                                // Scrolling AUDIO multicast setting selection
                                else if (sipAudMSetEdit == 2)
                                {
                                    // Exit Editing - Selection
                                    if (sipAudMSetIdx == 3)
                                    {
                                        // First GPIO scan
                                        if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                        {
                                            // Delay 0.1s
                                            msleep(100);
                                            // Exit PTT setting selection - Confirm selection
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                sipAudMSetIdx = 0;
                                                ricSubMenuSIPIdx = 0;
                                                subMenuSIPCnfg(ricSubMenuSIPIdx);

                                                initRicMenu = 0;
                                                sipAudMSetEdit = 0;
                                            }
                                        }
                                    }
                                    // Enable or disable AUDIO setting
                                    else if ((sipAudMSetIdx == 0) || (sipAudMSetIdx == 1))
                                    {
                                        // Check UP and DOWN press for saving AUDIO multicast setting process
                                        if (saveEnblDsbl == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Saves enable or disable setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Enable
                                                    if (sipAudMSetIdx == 0)
                                                    {
                                                        // Save enable or disable setting
                                                        subMenuEnblDsblStoredStat(0x04, 0x01);
                                                        saveEnblDsbl = 1;
                                                    }
                                                    // Disable
                                                    else
                                                    {
                                                        // Save enable or disable setting
                                                        subMenuEnblDsblStoredStat(0x04, 0x00);
                                                        saveEnblDsbl = 1;
                                                    }
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (saveEnblDsbl == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                saveEnblDsbl = 2;
                                            }
                                        }
                                        else if (saveEnblDsbl == 2)
                                        {
                                            // 3S delay
                                            sleep(3);
                                            sipAudMSetIdx = 0;
                                            subMenuEnblDsbl(sipAudMSetIdx, 0x04);

                                            initRicMenu = 0;
                                            saveEnblDsbl = 0;
                                        }
                                    }
                                    // Check current AUDIO multicast setting
                                    else if (sipAudMSetIdx == 2)
                                    {
                                        // Check UP and DOWN press for checking current stored AUDIO multicast setting
                                        if (chkAudMSet == 0)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Check current PTT setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    // Check current AUDIO setting
                                                    checkEnblDsblSet(0x04);
                                                    chkAudMSet = 1;
                                                }
                                            }
                                        }
                                        // Check UP and DOWN release state
                                        else if (chkAudMSet == 1)
                                        {
                                            // UP and DOWN confirm in release state
                                            if ((read_gpio(0x50,0,gpioUp) != 0x00) && (read_gpio(0x52,0,gpioDwn) != 0x00))
                                            {
                                                chkAudMSet = 2;
                                            }
                                        }
                                        // Start check UP and DOWN press action
                                        // Exit check current AUDIO multicast setting
                                        else if (chkAudMSet == 2)
                                        {
                                            // First GPIO scan
                                            if ((read_gpio(0x50,0,gpioUp) == 0x00) || (read_gpio(0x52,0,gpioDwn) == 0x00))
                                            {
                                                // Delay 0.1s
                                                msleep(100);
                                                // Exit check current MIC setting - Confirm selection
                                                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                                                {
                                                    sipAudMSetIdx = 0;
                                                    subMenuEnblDsbl(sipAudMSetIdx, 0x04);

                                                    initRicMenu = 0;
                                                    chkAudMSet = 0;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // Sub menu VOX configuration
                    else if (ricMenuIdx == 1)
                    {

                    }
                    // 1s delay before activate back UP and DOWN scrolling functions
                    // Below scroll UP and DOWN process are only valid during RIC settings menu
                    if ((initRicMenu == 1) && (saveSipId == 0) && (chkSipId == 0) && (delSipId == 0)
                        && (savePwdId == 0) && (chkSipPwd == 0) && (delSipPwd == 0)
                        && (saveIpAddr == 0) && (chkIpAddr == 0) && (delIpAddr == 0)
                        && (saveEnblDsbl == 0) && (chkPttSet == 0) && (chkMicSet == 0)
                        && (chkAudSet == 0) && (chkAudMSet == 0))
                    {
                         // Menu UP - Scroll UP once
                        if ((read_gpio(0x50,0,gpioUp) == 0x00) && (!upPressedOnce))
                        {
                            if (read_gpio(0x52,0,gpioDwn) != 0x00)
                            {
                                // RIC settings main menu
                                if ((ricSubMenuSIP == 0) && (sipIdEdit == 0) && (sipPwdEdit == 0)
                                    && (sipIpAddrEdit == 0) && (sipPttSetEdit == 0) && (sipMicSetEdit == 0)
                                    && (sipAudSetEdit == 0) && (sipAudMSetEdit == 0))
                                {
                                    if (ricMenuIdx != 0)
                                    {
                                        ricMenuIdx--;
                                        menuRIC(ricMenuIdx);
                                    }
                                }
                                // SIP settings main menu
                                else if ((ricSubMenuSIP != 0) && (sipIdEdit == 0) && (sipPwdEdit == 0)
                                    && (sipIpAddrEdit == 0) && (sipPttSetEdit == 0) && (sipMicSetEdit == 0)
                                    && (sipAudSetEdit == 0) && (sipAudMSetEdit == 0))
                                {
                                    if (ricSubMenuSIPIdx != 0)
                                    {
                                        ricSubMenuSIPIdx--;
                                        subMenuSIPCnfg(ricSubMenuSIPIdx);
                                    }
                                }
                                // SIP ID entering selection
                                else if (sipIdEdit != 0)
                                {
                                    if (sipIdIdx != 0)
                                    {
                                        sipIdIdx--;
                                        subMenuSIPId(sipIdIdx);
                                    }
                                }
                                // SIP password entering selection
                                else if (sipPwdEdit != 0)
                                {
                                    if (sipPwdIdx != 0)
                                    {
                                        sipPwdIdx--;
                                        subMenuSIPPswd(sipPwdIdx);
                                    }
                                }
                                // Asterisk IP address entering selection
                                else if (sipIpAddrEdit != 0)
                                {
                                    if (sipIpAddrIdx != 0)
                                    {
                                        sipIpAddrIdx--;
                                        subMenuSIPAstIp(sipIpAddrIdx);
                                    }
                                }
                                // Enable or disable PTT setting selection
                                else if (sipPttSetEdit != 0)
                                {
                                    if (sipPttSetIdx != 0)
                                    {
                                        sipPttSetIdx--;
                                        subMenuEnblDsbl(sipPttSetIdx, 0x01);
                                    }
                                }
                                // Enable or disable MIC setting selection
                                else if (sipMicSetEdit != 0)
                                {
                                    if (sipMicSetIdx != 0)
                                    {
                                        sipMicSetIdx--;
                                        subMenuEnblDsbl(sipMicSetIdx, 0x02);
                                    }
                                }
                                // Enable or disable AUDIO setting selection
                                else if (sipAudSetEdit != 0)
                                {
                                    if (sipAudSetIdx != 0)
                                    {
                                        sipAudSetIdx--;
                                        subMenuEnblDsbl(sipAudSetIdx, 0x03);
                                    }
                                }
                                // Enable or disable AUDIO multicast setting selection
                                else if (sipAudMSetEdit != 0)
                                {
                                    if (sipAudMSetIdx != 0)
                                    {
                                        sipAudMSetIdx--;
                                        subMenuEnblDsbl(sipAudMSetIdx, 0x04);
                                    }
                                }
                                upPressedOnce = 0x01;
                            }
                        }
                        // Clear flag once menu UP release
                        else if ((read_gpio(0x50,0,gpioUp) != 0x00) && (upPressedOnce))
                        {
                            upPressedOnce = 0x00;
                        }
                        // Menu DOWN - Scroll DOWN once
                        if ((read_gpio(0x52,0,gpioDwn) == 0x00) && (!dwnPressedOnce))
                        {
                            if (read_gpio(0x50,0,gpioUp) != 0x00)
                            {
                                // RIC settings main menu
                                if ((ricSubMenuSIP == 0) && (sipIdEdit == 0) && (sipPwdEdit == 0)
                                    && (sipIpAddrEdit == 0) && (sipPttSetEdit == 0) && (sipMicSetEdit == 0)
                                    && (sipAudSetEdit == 0) && (sipAudMSetEdit == 0))
                                {
                                    if (ricMenuIdx != 2)
                                    {
                                        ricMenuIdx++;
                                        menuRIC(ricMenuIdx);
                                    }
                                }
                                // SIP settings main menu
                                else if ((ricSubMenuSIP != 0) && (sipIdEdit == 0) && (sipPwdEdit == 0)
                                    && (sipIpAddrEdit == 0) && (sipPttSetEdit == 0) && (sipMicSetEdit == 0)
                                    && (sipAudSetEdit == 0) && (sipAudMSetEdit == 0))
                                {
                                    if (ricSubMenuSIPIdx != 9)
                                    {
                                        ricSubMenuSIPIdx++;
                                        subMenuSIPCnfg(ricSubMenuSIPIdx);
                                    }
                                }
                                // SIP ID entering selection
                                else if (sipIdEdit != 0)
                                {
                                    if (sipIdIdx != 12)
                                    {
                                        sipIdIdx++;
                                        subMenuSIPId(sipIdIdx);
                                    }
                                }
                                // SIP password entering selection
                                else if (sipPwdEdit != 0)
                                {
                                    if (sipPwdIdx != 12)
                                    {
                                        sipPwdIdx++;
                                        subMenuSIPPswd(sipPwdIdx);
                                    }
                                }
                                // Asterisk IP address entering selection
                                else if (sipIpAddrEdit != 0)
                                {
                                    if (sipIpAddrIdx != 13)
                                    {
                                        sipIpAddrIdx++;
                                        subMenuSIPAstIp(sipIpAddrIdx);
                                    }
                                }
                                // Enable or disable PTT setting selection
                                else if (sipPttSetEdit != 0)
                                {
                                    if (sipPttSetIdx != 3)
                                    {
                                        sipPttSetIdx++;
                                        subMenuEnblDsbl(sipPttSetIdx, 0x01);
                                    }
                                }
                                // Enable or disable MIC setting selection
                                else if (sipMicSetEdit != 0)
                                {
                                    if (sipMicSetIdx != 3)
                                    {
                                        sipMicSetIdx++;
                                        subMenuEnblDsbl(sipMicSetIdx, 0x02);
                                    }
                                }
                                // Enable or disable AUDIO setting selection
                                else if (sipAudSetEdit != 0)
                                {
                                    if (sipAudSetIdx != 3)
                                    {
                                        sipAudSetIdx++;
                                        subMenuEnblDsbl(sipAudSetIdx, 0x03);
                                    }
                                }
                                // Enable or disable AUDIO multicast setting selection
                                else if (sipAudMSetEdit != 0)
                                {
                                    if (sipAudMSetIdx != 3)
                                    {
                                        sipAudMSetIdx++;
                                        subMenuEnblDsbl(sipAudMSetIdx, 0x04);
                                    }
                                }
                                dwnPressedOnce = 0x01;
                            }
                        }
                        // Clear flag once menu DOWN release
                        else if ((read_gpio(0x52,0,gpioDwn) != 0x00) && (dwnPressedOnce))
                        {
                            dwnPressedOnce = 0x00;
                        }
                    }
                }
            }
            // Still inside main menu mode
            if ((initStartMenu == 1) &&  (initRicMenu == 1) && (temperatureMenu == 0) && (locIpAddrMenu == 0) && (wrldIpAddrMenu == 0) && (ricMenu == 0))
            {
                // Menu UP - Scroll UP once
                //if ((gpioUpRead) && (!upPressedOnce))
                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (!upPressedOnce))
                {
                    if (read_gpio(0x52,0,gpioDwn) != 0x00)
                    {
                        if (mainMenuIdx != 0)
                        {
                            mainMenuIdx--;
                            menuLCD(mainMenuIdx);
                        }
                        printf ("[%s %s] THD-LCD-MENU: UP GPIO ON\n", pCurrDates, pCurrTimes);
                        upPressedOnce = 0x01;
                    }
                }
                // Clear flag once menu UP release
                else if ((read_gpio(0x50,0,gpioUp) != 0x00) && (upPressedOnce))
                {
                    upPressedOnce = 0x00;
                }
                // Menu DOWN - Scroll DOWN once
                if ((read_gpio(0x52,0,gpioDwn) == 0x00) && (!dwnPressedOnce))
                {
                    //if (!gpioUpRead)
                    if (read_gpio(0x50,0,gpioUp) != 0x00)
                    {
                        if (mainMenuIdx != 5)
                        {
                            mainMenuIdx++;
                            menuLCD(mainMenuIdx);
                        }
                        printf ("[%s %s] THD-LCD-MENU: DOWN GPIO ON\n", pCurrDates, pCurrTimes);
                        dwnPressedOnce = 0x01;
                    }
                }
                // Clear flag once menu DOWN release
                else if ((read_gpio(0x52,0,gpioDwn) != 0x00) && (dwnPressedOnce))
                {
                    dwnPressedOnce = 0x00;
                }
            }
        }
        // NO GPIO read process
        else
        {
            initStartMenu = 0;
            firstMenuCnt = 0;

            initRicMenu = 0;
            ricMenuCnt = 0;
        }
        // Release memory allocation
        free(pCurrDates);
        free(pCurrTimes);

        //pthread_mutex_unlock(&lcdMenuMutex);
        //sleep(1);
        msleep(100);
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
	int menuSelCnt = 0;
	int exitMainMenuCnt = 0;

	unsigned char lcdDateTime = 0x01;
    unsigned char defaultDisp = 0x00;

    char *pCurrDate;
	char *pCurrTime;
	char datetimebuff[16];
	char dummyBuff[16];

	dummyBuff[0] = ' ';dummyBuff[1] = ' ';dummyBuff[2] = ' ';dummyBuff[3] = ' ';
    dummyBuff[4] = ' ';dummyBuff[5] = ' ';dummyBuff[6] = ' ';dummyBuff[7] = ' ';
    dummyBuff[8] = ' ';dummyBuff[9] = ' ';dummyBuff[10] = ' ';dummyBuff[11] = ' ';
    dummyBuff[12] = ' ';dummyBuff[13] = ' ';dummyBuff[14] = ' ';dummyBuff[15] = ' ';

	memset(&act, 0, sizeof(act));
	memset(&datetimebuff, 0, sizeof(datetimebuff));
	memset(&sipIdStored, 0, sizeof(sipIdStored));
	memset(&sipPwdStored, 0, sizeof(sipPwdStored));
	memset(&sipAstIpAddr, 0, sizeof(sipAstIpAddr));

	act.sa_sigaction = sigkill_handler;
	act.sa_flags = SA_SIGINFO;

	sigaction(SIGTERM, &act, NULL);  // For handling signal generated by kill command
	signal(SIGINT, sigint_handler);  // For handling signal generated by interrupt ctrl-c
	signal(SIGSEGV, sigint_handler); // For handling signal generated by segmentation violation
	signal(SIGUSR1, sigint_handler); // For handling kill -USR1 action, for KILL the daemon manually

    // Initialize mutex for thread synchronization
	if (pthread_mutex_init(&lcdMenuMutex, NULL) != 0)
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
	// Initialize GPIO50 - 0x50
    // Initialize failed!
    if (read_gpio(0x50,0,gpioUp) == 0xff)
    {
        printf ("[%s %s] MAIN-LOOP: UP GPIO initialization FAILED!\n", pCurrDate, pCurrTime);
    }
    // Initialize successfull
    else
    {
        // Only enable and initialize this GPIO once
        if (gpioUp == 0x00)
        {
            gpioUp = 0x01;
        }
        printf ("[%s %s] MAIN-LOOP: UP GPIO initialization SUCCESSFULL\n", pCurrDate, pCurrTime);
    }
    // Initialize GPIO50 - 0x50
    // Initialize failed!
    if (read_gpio(0x52,0,gpioDwn) == 0xff)
    {
        printf ("[%s %s] MAIN-LOOP: DOWN GPIO initialization FAILED!\n", pCurrDate, pCurrTime);
    }
    // Initialize successfull
    else
    {
        // Only enable and initialize this GPIO once
        if (gpioDwn == 0x00)
        {
            gpioDwn = 0x01;
        }
        printf ("[%s %s] MAIN-LOOP: DOWN GPIO initialization SUCCESSFULL\n", pCurrDate, pCurrTime);
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
	//printlcd (" Version: 1.0.1 ", 16);
	printlcd ("      ROIP      ", 16);

    // Create thread for GPIO read and LCD menu functionalities
    // Failed
    if (pthread_create(&tidGpioLcd,NULL,thread_lcd_menu,NULL) < 0)
    {
        printf ("[%s %s] MAIN-LOOP: Create thread GPIO and LCD menu FAILED!\n", pCurrDate, pCurrTime);
    }

    // Main process loop
	// Process interval at the main loop will used custom delay
	mainLoop:
        msleep(500);
        //msleep(100);

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
		// Previously event exit main menu has triggered, go to default LCD display mode
        if (exitMainMenu == 0x01)
        {
            // Display 3s exit main menu info
            if (exitMainMenuCnt == 0)
            {
                setCursor (0, 0);
                printlcd ("Exit Main Menu..", 16);
                setCursor(0, 1);
                printlcd (dummyBuff, 16);
            }
            exitMainMenuCnt++;
            // After 3s goes back to default LCD display
            if (exitMainMenuCnt == 6)
            {
                exitMainMenu = 0x00;
                mainMenuMode = 0x00;

                exitMainMenuCnt = 0;
            }
        }
        // Checking UP and DOWN GPIO for entering main menu mode
        else
        {
            // Normal mode
            if (!mainMenuMode)
            {
                menuSelCnt++;
                // After 3s check whether UP and DOWN GPIO are entering the menu mode
                if (menuSelCnt == 6)
                //if (menuSelCnt == 30)
                {
                    // After 3s UP and DOWN GPIO was pressed
                    if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                    {
                        clear(); // Clear first LCD previous contents
                        menuLCD(mainMenuIdx);
                        mainMenuMode = 0x01;
                    }
                    menuSelCnt = 0;
                }
            }
        }
        /*
		// Normal mode
        if (!mainMenuMode)
        {
            menuSelCnt++;
            // After 3s check whether UP and DOWN GPIO are entering the menu mode
            if (menuSelCnt == 6)
            //if (menuSelCnt == 30)
            {
                // After 3s UP and DOWN GPIO was pressed
                if ((read_gpio(0x50,0,gpioUp) == 0x00) && (read_gpio(0x52,0,gpioDwn) == 0x00))
                {
                    clear(); // Clear first LCD previous contents
                    menuLCD(mainMenuIdx);
                    mainMenuMode = 0x01;
                }
                menuSelCnt = 0;
            }
        }
        */
        // Default normal mode
        if (!mainMenuMode)
        {
            if (mainMenuIdx != 0)
            {
                mainMenuIdx = 0;
            }
            // Show daemon version number
            if (showVersion)
            {
                verCnt++;
                // Every 30s, print the IO interface daemon version - 30/0.5
                if (verCnt == 60)
                //if (verCnt == 300)
                {
                    #ifdef DEBUG_FILE
                    fprintf(fpDebug, "[%s %s] MAIN-LOOP: IO-Interface Version: 1.0.1\n", pCurrDate, pCurrTime);
                    #else
                    printf("[%s %s] MAIN-LOOP: IO-Interface Version: 1.0.1\n", pCurrDate, pCurrTime);
                    #endif // DEBUG_FILE
                    verCnt = 0;
                }
            }
            if (!defaultDisp)
            {
                defaultDisp = 0x01;
            }
            else
            {
                defaultDisp = 0x00;

                lcdCnt++; // Seconds counter
                // Every 5s change LCD display contents
                // Shows current date and time
                if (lcdCnt == 10)
                //if (lcdCnt == 50)
                {
                    lcdDateTime = 0x00;

                    clear(); // Clear first LCD previous contents
                    setCursor (0, 0);
                    printlcd ("   MASURI+SCH   ", 16);
                    setCursor(0, 1);
                    //printlcd (" Version: 1.0.1 ", 16);
                    printlcd ("      ROIP      ", 16);
                }
                else if (lcdCnt == 20)
                //else if (lcdCnt == 100)
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
            }
        }
        /*
        // Previously event exit main menu has triggered, go to default LCD display mode
        if (exitMainMenu == 0x01)
        {
            exitMainMenu = 0x00;
            mainMenuMode = 0x00;
        }
        */
        // Release memory allocation
		free(pCurrDate);
		free(pCurrTime);
	goto mainLoop;
	// Clear GPIO API
    deactivate_gpio();
	return 0;
}
