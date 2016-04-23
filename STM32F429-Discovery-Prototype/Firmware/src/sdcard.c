#include <string.h>
#include "sdcard.h"
#include "ch.h"
#include "hal.h"
#include "ff.h"

//MMC SPI Setup
//SPI4 MMC1
#define MMC_DRIVER (&MMCD1)
#define MMC_SPI_DRIVER &SPID4
#define MMC_SPI_PORT GPIOE
#define MMC_SCK_PAD  2  //PE2
#define MMC_MISO_PAD 5 //PE5
#define MMC_MOSI_PAD 6 //PE6
#define MMC_CS_PAD     4        // PE4 --  0 = chip selected
#define MMC_CD_PAD  3       // PE3

//SPI setup adjust " SPI_BaudRatePrescaler_X" to set SPI speed.
//Peripheral Clock 42MHz SPI2 SPI3
//Peripheral Clock 84MHz SPI1                                SPI1        SPI2/3
#define SPI_BaudRatePrescaler_2         ((uint16_t)0x0000) //  42 MHz      21 MHZ
#define SPI_BaudRatePrescaler_4         ((uint16_t)0x0008) //  21 MHz      10.5 MHz
#define SPI_BaudRatePrescaler_8         ((uint16_t)0x0010) //  10.5 MHz    5.25 MHz
#define SPI_BaudRatePrescaler_16        ((uint16_t)0x0018) //  5.25 MHz    2.626 MHz
#define SPI_BaudRatePrescaler_32        ((uint16_t)0x0020) //  2.626 MHz   1.3125 MHz
#define SPI_BaudRatePrescaler_64        ((uint16_t)0x0028) //  1.3125 MHz  656.25 KHz
#define SPI_BaudRatePrescaler_128       ((uint16_t)0x0030) //  656.25 KHz  328.125 KHz
#define SPI_BaudRatePrescaler_256       ((uint16_t)0x0038) //  328.125 KHz 164.06 KHz

#define DEBOUNCE_INTERVAL               10
#define POLLING_DELAY                   10

/* MMC/SD over SPI driver configuration.*/
MMCDriver MMCD1;
/* Maximum speed SPI configuration (18MHz, CPHA=0, CPOL=0, MSb first).*/
static SPIConfig hs_spicfg = {NULL, MMC_SPI_PORT, MMC_CS_PAD, 0};

/* Low speed SPI configuration (281.250kHz, CPHA=0, CPOL=0, MSb first).*/
static SPIConfig ls_spicfg = {NULL, MMC_SPI_PORT, MMC_CS_PAD, SPI_BaudRatePrescaler_256}; //SPI_CR1_BR_2 | SPI_CR1_BR_1};
static MMCConfig mmccfg = {MMC_SPI_DRIVER, &ls_spicfg, &hs_spicfg};

// Stuff that lacks comments
static virtual_timer_t _tmr;
static unsigned _cnt_debounce;
static FATFS _SDC_FS;
static bool _fsReady = false;

//Setup MMC Event Handlers so we can talk to MMC without calling from I class function
static const evhandler_t evhndl[] = {
    _sdcardHandlerInsert,
    _sdcardHandlerRemove
};

/**
 * @brief   Card event sources.
 */
static event_source_t inserted_event, removed_event;

// SD-Card insert event handler
void _sdcardHandlerInsert(eventid_t id)
{
    (void)id;
	FRESULT err;

	// On insertion SDC initialization and FS mount.
	if (mmcConnect(MMC_DRIVER)) {
		return;
	}

	err = f_mount(&_SDC_FS, "/", 1);
	if (err != FR_OK) {
		mmcDisconnect(MMC_DRIVER);
		return;
	}
	_fsReady = true;
}

// SD-Card remove event handler
void _sdcardHandlerRemove(eventid_t id)
{
    (void)id;

	mmcDisconnect(MMC_DRIVER);
	_fsReady = false;
}

// Insertion monitor timer callback
static void _tmrfunc(void *p)
{
	BaseBlockDevice *bbdp = p;

	chSysLockFromISR();
	if (_cnt_debounce > 0) {
		if (blkIsInserted(bbdp)) {
			if (--_cnt_debounce == 0) {
				chEvtBroadcastI(&inserted_event);
			}
		}
		else
			_cnt_debounce = DEBOUNCE_INTERVAL;
	}
	else {
		if (!blkIsInserted(bbdp)) {
			_cnt_debounce = DEBOUNCE_INTERVAL;
			chEvtBroadcastI(&removed_event);
		}
	}

	chVTSetI(&_tmr, MS2ST(POLLING_DELAY), _tmrfunc, bbdp);
	chSysUnlockFromISR();
}

// Initialize the insertion monitor timer
static void _tmr_init(void *p)
{
    chEvtObjectInit(&inserted_event);
    chEvtObjectInit(&removed_event);

	chSysLock();

	_cnt_debounce = DEBOUNCE_INTERVAL;
	chVTSetI(&_tmr, MS2ST(POLLING_DELAY), _tmrfunc, p);

	chSysUnlock();
}

bool sdcardInit(void)
{
	_fsReady = false;

	//Event listeners for MMC Insert/Removed
    event_listener_t el0, el1;

	// Pin modes
	palSetPadMode(GPIOE, 3, PAL_MODE_INPUT);
	palSetPadMode(MMC_SPI_PORT, MMC_SCK_PAD, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
	palSetPadMode(MMC_SPI_PORT, MMC_MISO_PAD, PAL_MODE_ALTERNATE(5));
	palSetPadMode(MMC_SPI_PORT, MMC_MOSI_PAD, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
	palSetPadMode(MMC_SPI_PORT, MMC_CS_PAD, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
	palSetPadMode(MMC_SPI_PORT, MMC_CD_PAD, PAL_MODE_INPUT_PULLUP);
	palSetPad(MMC_SPI_PORT, MMC_CS_PAD);

	// Init SD/MMC Driver
	mmcObjectInit(MMC_DRIVER);
	mmcStart(MMC_DRIVER, &mmccfg);

	// Manually call the insertion handler in case of the card is already present
	_sdcardHandlerInsert(0);

	// Activates the card insertion monitor.
	_tmr_init(MMC_DRIVER);

	//Register Insert/Remove Events after everything is initialized
	chEvtRegister(&inserted_event, &el0, 0);
    chEvtRegister(&removed_event, &el1, 1);

	return true;
}

bool sdcardReady(void)
{
	return _fsReady;
}

void sdCardEvent(void){
    chEvtDispatch(evhndl, chEvtWaitOneTimeout(ALL_EVENTS, MS2ST(500)));
}
