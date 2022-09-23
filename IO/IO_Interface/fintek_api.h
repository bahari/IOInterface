#ifndef __FINTEK_API_H__
#define __FINTEK_API_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <poll.h>

#define BIT(x) (1ULL << (x))
#define SHIFT(x,y) ((x) << (y))
#define MIN(x,y) ( ((x) > (y)) ? (y) : (x) )
#define MAX(x,y) ( ((x) > (y)) ? (x) : (y) )

#define MAX_GPIO_SET 8
#define MAX_GPIO (16 * MAX_GPIO_SET)
#define CHECK_RET(x) {int tmp_ret; if(tmp_ret=x) { fprintf(stderr, #x" line: %d error: %d\n", __LINE__, tmp_ret);return 0;} }
#define MAX_IC (16)
#define MAX_PATHNAME_SIZE 64

#define DEBUG_FUN \
	do {	\
		if (get_debug()) \
			fprintf(stderr, "%s: %d\n", __func__, __LINE__); \
	} while (0);

#define DEBUG(x) \
	do {	\
		if (get_debug()) {\
			x; \
		}\
	} while (0);

typedef enum {
	eGPIO_Mode_Disable,
	eGPIO_Mode_Enable,
	eGPIO_Mode_Invalid,
} eGPIO_Mode;

typedef enum {
	eGPIO_Drive_Mode_OpenDrain,
	eGPIO_Drive_Mode_Pushpull,
	eGPIO_Drive_Mode_Invalid,
} eGPIO_Drive_Mode;

typedef enum {
	eGPIO_Direction_In,
	eGPIO_Direction_Out,
	eGPIO_Direction_Invalid,
} eGPIO_Direction;

typedef enum {
	eGPIO_Pull_Low,
	eGPIO_Pull_High,
	eGPIO_Pull_Disable,
	eGPIO_Pull_Invalid,
} eGPIO_Pull_Mode;

typedef enum {
	eSIO_TYPE_SIO = 0,
	eSIO_TYPE_F71808A = eSIO_TYPE_SIO,
	eSIO_TYPE_F71869A,
	eSIO_TYPE_F81866,
	eSIO_TYPE_F75113,
	eSIO_TYPE_F81801,
	eSIO_TYPE_F81768,
	eSIO_TYPE_F81803 = eSIO_TYPE_F81768,
	eSIO_TYPE_F81966,
	eSIO_TYPE_F81804 = eSIO_TYPE_F81966,

	eSIO_TYPE_HID,
	eSIO_TYPE_F75114 = eSIO_TYPE_HID,
	eSIO_TYPE_F75115,

	eSIO_TYPE_PCI,
	eSIO_TYPE_F81504 = eSIO_TYPE_PCI,
	eSIO_TYPE_F81508,
	eSIO_TYPE_F81512,

	eSIO_TYPE_USB,
	eSIO_TYPE_F81534A = eSIO_TYPE_USB,

	eSIO_TYPE_UNKNOWN,
	eSIO_TYPE_INVALID,
} eSIO_TYPE;

typedef enum {
	eError_NoError = 0,
} eError_Type;

typedef struct {
	int vid;
	int pid;
	int wdt_count;
	int gpio_count;
	unsigned char gpio_range[MAX_GPIO / 8];	// 16x8 can afford to 128 gpio
	void *fun_array;
} sProductData, *psProductData;

typedef struct {
	unsigned int vid;
	unsigned int pid;
	eSIO_TYPE ic_type;

	unsigned char ic_port;
	unsigned char key;
	char path_name[MAX_PATHNAME_SIZE];
	char device_arch[MAX_PATHNAME_SIZE];

	void *next;
} sFintek_sio_data, *psFintek_sio_data;

#ifdef __cplusplus
extern "C" {
#endif

	unsigned char READ_IC(unsigned long reg);
	void WRITE_IC(unsigned long reg, unsigned char data);
	void WRITE_MASK_IC(unsigned char reg, unsigned char mask,
			   unsigned char data);

	int init_fintek_sio(eSIO_TYPE eType, int index,
			    psFintek_sio_data sio_data);
	void ActiveSIO(unsigned char port, unsigned char key);
	void DeactiveSIO(unsigned char port);

	int get_device_list(eSIO_TYPE eType, psFintek_sio_data * head);

	int _EnableGPIO(unsigned int uIdx, eGPIO_Mode eMode);

	int _SetGpioOutputEnableIdx(unsigned int uIdx, eGPIO_Direction eMode);
	int _GetGpioOutputEnableIdx(unsigned int uIdx, eGPIO_Direction * eMode);

	int _SetGpioOutputDataIdx(unsigned int uIdx, unsigned int uValue);
	int _SetGpioGroupOutputDataIdx(unsigned int set, unsigned int uValue);
	int _GetGpioOutputDataIdx(unsigned int uIdx, unsigned int *uValue);

	int _GetGpioInputDataIdx(unsigned int uIdx, unsigned int *uValue);
	int _GetGpioGroupInputDataIdx(unsigned int set, unsigned int *uValue);

	int _SetGpioDriveEnable(unsigned int uIdx, eGPIO_Drive_Mode eMode);
	int _GetGpioDriveEnable(unsigned int uIdx, eGPIO_Drive_Mode * eMode);

	int _SetGpioPullMode(unsigned int uIdx, eGPIO_Pull_Mode eMode);
	int _GetGpioPullMode(unsigned int uIdx, eGPIO_Pull_Mode * eMode);

	int SetWdtConfiguration(int iTimerCnt, int iClkSel, int iPulseMode,
				int iUnit, int iActive, int iPulseWidth);
	int SetWdtEnable();
	int SetWdtDisable();

	int GetWdtTimeoutStatus(int *Status, int *RemainTime);
	int SetWdtIdxConfiguration(int idx, int iTimerCnt, int iClkSel,
				   int iPulseMode, int iUnit, int iActive,
				   int iPulseWidth);

	int SetWdtIdxEnable(int idx);
	int SetWdtIdxDisable(int idx);

	int GetWdtIdxTimeoutStatus(int idx, int *Status, int *RemainTime);

	int GetHWMONAddr(unsigned int *addr);
	int SelectI2CChannel(unsigned int ch);
	int WriteI2CData(unsigned int addr, unsigned int data);
	int ReadI2CData(unsigned int addr, unsigned int *data);

	int InitEEPROM(void);
	int SetEEPROMValue(unsigned int addr, unsigned int data);
	int GetEEPROMValue(unsigned int addr, unsigned int *data);

	int SpiSetCs(int idx, int en);
	int SpiSetPolPha(int idx, int pol, int pha);
	int SpiReadData(int idx, unsigned char *data);
	int SpiWriteData(int idx, unsigned char data);

	void set_debug(int on);
	int get_debug(void);
	char *getFintekLibVersion(void);

	unsigned char IOReadByte(unsigned int addr);
	void IOWriteByte(unsigned int addr, unsigned char data);

	int _GPIO_Check_Index(int logicIndex);
	int _GPIO_Check_Set(int set);
	int _WDT_count();

#ifdef __cplusplus
}
#endif
#endif
