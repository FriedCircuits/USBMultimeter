#include "ch.h"
#include "hal.h"
#include "gui.h"
#include "gfx.h"
#include "lib/ina219/ina219.h"
#include "lib/util/stm32_util.h"
#include "usbtester.h"
#include "ff.h"
#include "usbcfg.h"
#include "chprintf.h"
#include "sdcard.h"
#include "cmd.h"
#include "usbdata.h"

// Global variables
bool updateDisplayValues = false;
volatile float shuntvoltage = 0;
volatile float busvoltage = 0;
volatile float current_mA = 0;
volatile float loadvoltage = 0;
volatile float milliwatthours = 0;
volatile float milliamphours = 0;

// Keep track of peak/min significant values:
volatile float peakCurrent = 0;
volatile float voltageAtPeakCurrent = 0;
volatile float minVoltage = 10;
volatile float currentAtMinVoltage = 0;
// (no need, we can compute it) volatile float peakPower = 0;
volatile float voltageAtPeakPower = 0;
volatile float currentAtPeakPower = 0;

// Keep track of variation of volts/amps over the serial refresh period
volatile float rpPeakCurrent = 0;
volatile float rpMinCurrent = 0;
volatile float rpPeakLoadVolt = 0;
volatile float rpMinLoadVolt = 0;
volatile float rpAvgCurrent = 0;
volatile float rpAvgLoadVolt = 0;
uint32_t rpSamples = 1;

// OS Objects
// static virtual_timer_t vtINA;
RTCDateTime timespec;
bool use12HR = false;
bool updateGraph = true;
bool updateSettingTime = false;
struct tm startTime;

// Function prototypes
void startupInit(void);
static void createSettingsFrame(void);
void updateGraphData(float reading, uint8_t graph);
int mapI(int x, int in_min, int in_max, int out_min, int out_max);
float mapf(float x, float in_min, float in_max, float out_min, float out_max);
void setLabelTime(GHandle gh, bool timeDate);
void setStartTime(void);
void drawGraphLines(GHandle gh);
void displayGraph(GHandle gh, uint8_t graph);
void setActiveGraph(uint8_t graph);
void graphInit(void);


static THD_WORKING_AREA(ledThread, 128);
static THD_FUNCTION(ThreadLED, arg)
{
	(void)arg;

	chRegSetThreadName("HeartBeat");

	while (true) {
		palSetPad(GPIOG, GPIOG_LED3_GREEN);
		chThdSleepMilliseconds(100);
		palClearPad(GPIOG, GPIOG_LED3_GREEN);
		chThdSleepMilliseconds(1000);
	}
}

static THD_WORKING_AREA(gfxvalThread, 2048);
static THD_FUNCTION(ThreadGFXVal, arg)
{
	(void)arg;

	chRegSetThreadName("GFXVal");

	while (true) {
		gwinSetText(ghlblV, util_floatToString(loadvoltage, UNIT_V), TRUE);
		gwinSetText(ghlblA, util_floatToString(current_mA, UNIT_MA), TRUE);
		gwinSetText(ghlblW, util_floatToString(((current_mA*loadvoltage)/1000), UNIT_W), TRUE);
		//gwinSetText(ghlblAH, util_floatToString(milliamphours, UNIT_MAH), TRUE);

		if ((scaleChanged) && (activeGraph != 3)) {
			char str[7];
			snprintf(str, 7, "%d%s", autoscale_limits[activeGraph][graph_MAX[activeGraph]], units[graphUnits[activeGraph]]);
			gwinSetText(ghBtnScale, str, TRUE);
			drawGraphLines(ghContainerGraphC);
			scaleChanged = false;
		}

		if (activeGraph == 3) {
			char str[7];
			snprintf(str, 7, "%d%s", autoscale_limits[0][graph_MAX[0]], units[graphUnits[0]]);
			gwinSetText(ghBtnScale, str, TRUE);
			scaleChanged = false;
			snprintf(str, 7, "%d%s", autoscale_limits[1][graph_MAX[1]], units[graphUnits[1]]);
			gwinDrawString(ghContainerGraphCV_2, gwinGetWidth(ghContainerGraphCV_2)-25, 5, str);
			drawGraphLines(ghContainerGraphCV_1);
			drawGraphLines(ghContainerGraphCV_2);
		}

		chThdSleepMilliseconds(500);
	}
}

static THD_WORKING_AREA(graphThread, 2048);
static THD_FUNCTION(ThreadGRAPH, arg)
{
	(void)arg;

	chRegSetThreadName("Graph");

	while (true) {
		updateGraphData(current_mA, 0);
		updateGraphData(loadvoltage, 1);
		updateGraphData(((current_mA*loadvoltage)/1000), 2);

		if (activeGraph == 0) {
			displayGraph(ghContainerGraphC, 0);
		}

		if (activeGraph == 1) {
			displayGraph(ghContainerGraphV, 1);
		}

		if (activeGraph == 2) {
			displayGraph(ghContainerGraphW, 2);
		}

		if (activeGraph == 3) {
			displayGraph(ghContainerGraphCV_1, 0);
			displayGraph(ghContainerGraphCV_2, 1);
		}

		chThdSleepMilliseconds(GRAPH_Refresh);
	}
}


static THD_WORKING_AREA(gfxEventThread, 2048);
static THD_FUNCTION(ThreadGFXEvent, arg)
{
	(void)arg;

	chRegSetThreadName("GFXEvent");

	GEvent* pe;
	static RTCDateTime timespec;
	struct tm timeinfo;
	struct tm tim;
	struct tm *canary;
	static time_t unix_time;
	while (true) {
		pe = geventEventWait(&gl, TIME_INFINITE);
		switch (pe->type) {
			case GEVENT_GWIN_BUTTON:
				if (((GEventGWinButton*)pe)->gwin == ghBtnSettings) {
					createSettingsFrame();
					gwinShow(ghFrame1);
					updateGraph = false;
					updateSettingTime = true;
				} else if (((GEventGWinButton*)pe)->gwin == ghBtnSave) {
					timeinfo.tm_hour = atoi(gwinListGetSelectedText(ghListHour));
					timeinfo.tm_min = atoi(gwinListGetSelectedText(ghListMin));
					timeinfo.tm_sec = 0;
					timeinfo.tm_mday = atoi(gwinListGetSelectedText(ghListDay));
					timeinfo.tm_mon = gwinListGetSelected(ghListMonth);
					timeinfo.tm_year = atoi(gwinListGetSelectedText(ghListYear))-1900;
					unix_time = mktime(&timeinfo);
					canary = localtime_r(&unix_time, &tim);
					osalDbgCheck(&tim == canary);
					rtcConvertStructTmToDateTime(&tim, 0, &timespec);
					rtcSetTime(RTC_DRIVER, &timespec);
				} else if (((GEventGWinButton*)pe)->gwin == ghBtnTime) {
					updateGraph = false;
					gwinShow(ghListFreq);
				}
				break;

			case GEVENT_GWIN_CHECKBOX:
				if (((GEventGWinCheckbox*)pe)->gwin == ghCheckBoxHR) {
					if (((GEventGWinCheckbox*)pe)->isChecked)
						use12HR = true;
					else
						use12HR = false;
				}
				break;

			case GEVENT_GWIN_CLOSE:
				updateGraph = true;
				updateSettingTime = false;
				switch (activeGraph) {
					case 0:
						drawGraphLines(ghContainerGraphC);
						break;

					case 1:
						drawGraphLines(ghContainerGraphV);
						break;

					case 2:
						drawGraphLines(ghContainerGraphW);
						break;

					case 3:
						drawGraphLines(ghContainerGraphCV_1);
						drawGraphLines(ghContainerGraphCV_2);
						break;

					default:
						break;
				}
				break;

			case GEVENT_GWIN_RADIO:
				activeGraph = atoi(gwinGetText(((GEventGWinRadio *)pe)->gwin));
				setActiveGraph(activeGraph);
				break;

			case GEVENT_GWIN_LIST:
				if (((GEventGWinList*)pe)->gwin == ghListFreq)  {
					char str[5];
					uint8_t val = atoi(gwinListGetSelectedText(ghListFreq));
					GRAPH_Refresh = ((1.0/val)*1000);
					snprintf(str, 5, "%d%s", val, "Hz");
					gwinSetText(ghBtnTime, str, true);
					chThdSleepMilliseconds(10);
					gwinHide(ghListFreq);
					updateGraph = true;

					switch (activeGraph) {
					case 0:
						drawGraphLines(ghContainerGraphC);
						break;

					case 1:
						drawGraphLines(ghContainerGraphV);
						break;

					case 2:palSetPadMode(GPIOA, 5, PAL_MODE_OUTPUT_PUSHPULL); palClearPad(GPIOA, 5);
						drawGraphLines(ghContainerGraphW);
						break;

					case 3:
						drawGraphLines(ghContainerGraphCV_1);
						drawGraphLines(ghContainerGraphCV_2);
						break;

					default:
						break;
					}

				}

				break;
			default:
				break;
		}

		chThdSleepMilliseconds(500);
	}
}

static THD_WORKING_AREA(inaThread, 256);
static THD_FUNCTION(ThreadINA, arg)
{
	(void)arg;

	chRegSetThreadName("INA");
	systime_t time = chVTGetSystemTimeX();     // T0

	while (true) {
		time += US2ST(READFREQ);
		//palSetPad(GPIOG, GPIOG_LED4_RED);
		//palTogglePad(GPIOG, GPIOG_LED4_RED);
		busvoltage=ina219GetBusVoltage_V();
		current_mA=ina219GetCurrent_mA();
		shuntvoltage = ina219GetShuntVoltage_mV();
		loadvoltage = busvoltage + (shuntvoltage / 1000);
		milliwatthours += busvoltage*current_mA*READFREQ/1e6/3600; // 1 Wh = 3600 joules
		milliamphours += current_mA*READFREQ/1e6/3600;

		// Update peaks, min and avg during our serial refresh period:
		if (current_mA > rpPeakCurrent)
			rpPeakCurrent = current_mA;
		if (current_mA < rpMinCurrent)
			rpMinCurrent = current_mA;
		if (loadvoltage > rpPeakLoadVolt)
			rpPeakLoadVolt  = loadvoltage;
		if (loadvoltage < rpMinLoadVolt)
			rpMinLoadVolt = loadvoltage;
		rpAvgCurrent = (rpAvgCurrent*rpSamples + current_mA)/(rpSamples+1);
		rpAvgLoadVolt = (rpAvgLoadVolt*rpSamples + loadvoltage)/(rpSamples+1);
		rpSamples++;

		// Update absolute peaks and mins
		if (current_mA > peakCurrent) {
			peakCurrent = current_mA;
			voltageAtPeakCurrent = loadvoltage;
		}

		if (loadvoltage < minVoltage) {
			minVoltage = loadvoltage;palSetPadMode(GPIOA, 5, PAL_MODE_OUTPUT_PUSHPULL); palClearPad(GPIOA, 5);
			currentAtMinVoltage = current_mA;
		}

		//palClearPad(GPIOG, GPIOG_LED4_RED);
		chThdSleepUntil(time);
	}
}

static THD_WORKING_AREA(timeThread, 1024);
static THD_FUNCTION(ThreadTIME, arg)
{
	(void)arg;

	chRegSetThreadName("Time");
	struct tm timp;
	char str_date[100];

	while (true) {
		if(updateSettingTime) {
			setLabelTime(ghlblDateTime, true);
		}

		rtcGetTime(RTC_DRIVER, &timespec);
		rtcConvertDateTimeToStructTm(&timespec, &timp,0);
		//https://www.iar.com/support/resources/articles/using-c-standard-library-time-and-clock-functions/
		//http://www.cplusplus.com/forum/general/57329/
		int timeElap = difftime(mktime(&timp),mktime(&startTime));
		uint8_t hour = timeElap/3600;
		uint16_t second = timeElap % 3600;
		uint8_t minute = second/60;
		second %= 60;
		//timp.tm_sec = second;
		//timp.tm_min = minute;
		//timp.tm_hour = hour;
		//strftime(str_date, sizeof(str_date), "%X", &timp); //%x for date http://www.tutorialspoint.com/c_standard_library/c_function_strftime.htm
		snprintf(str_date, 9, "%02d:%02d:%02d", hour, minute, second);
		gwinSetText(ghlblElapsed, str_date, TRUE);
		chThdSleepMilliseconds(1000);
	}
}

int main(void)
{
	// Initialize ChibiOS
	halInit();
	chSysInit();

	// Initialize uGFX
	gfxInit();

	// Initialize our own stuff
	startupInit();

	// Get the command prompt (via USB) up and running
	cmdCreate();

	// Get the SD-Card stuff done
	sdcardInit();

	while (true) {
        cmdManage();
		if(SDU1.config->usbp->state == USB_ACTIVE){
			gwinShow(ghImageUSB);
		} else {
			gwinHide(ghImageUSB);
		}

		// Until the statusbar widget is ready...
		gwinSetVisible(ghImageSDC, sdcardReady());

        sdCardEvent();

		chThdSleepMilliseconds(500);
	}
}

void startupInit()
{
    usbdataEnable();

	gdispSetBacklight(50);
	gdispSetContrast(50);

	geventListenerInit(&gl);
	gwinAttachListener(&gl);

	// Setup UI objects and show splash
	guiCreate();
	gwinProgressbarSetResolution(ghProgbarLoad, 10);
	gwinProgressbarStart(ghProgbarLoad, 300);
	graphInit();

	// Setup INA219 I2C Current Sensor
	ina219Init();

	sduObjectInit(USB_SERIAL_DRIVER);
	sduStart(USB_SERIAL_DRIVER, &serusbcfg);

	/*
	 * Activates the USB driver and then the USB bus pull-up on D+.
	 * Note, a delay is inserted in order to not have to disconnect the cable
	 * after a reset.
	 */
	usbDisconnectBus(serusbcfg.usbp);
	chThdSleepMilliseconds(1000);
	usbStart(serusbcfg.usbp, &usbcfg);
	usbConnectBus(serusbcfg.usbp);

	setStartTime();

	chThdCreateStatic(ledThread, sizeof(ledThread), LOWPRIO, ThreadLED, NULL);
	chThdCreateStatic(inaThread, sizeof(inaThread), HIGHPRIO, ThreadINA, NULL);
	chThdCreateStatic(gfxvalThread, sizeof(gfxvalThread), NORMALPRIO, ThreadGFXVal, NULL);
	chThdCreateStatic(timeThread, sizeof(timeThread), NORMALPRIO, ThreadTIME, NULL);
	chThdCreateStatic(gfxEventThread, sizeof(gfxEventThread), NORMALPRIO, ThreadGFXEvent, NULL);

	chThdSleepMilliseconds(2000);

	chThdCreateStatic(graphThread, sizeof(graphThread), NORMALPRIO+5, ThreadGRAPH, NULL);
	// Destroy splay screen and show main page
	gwinDestroy(ghContainerPageSplash);
	guiShowPage(1);
}

static void createSettingsFrame(void)
{
	GWidgetInit wi;

	gwinWidgetClearInit(&wi);
	wi.g.show = FALSE;

	wi.g.width = 320;
	wi.g.height = 190;
	wi.g.y = 25;
	wi.g.x = 0;
	wi.text = "Settings";
	ghFrame1 = gwinFrameCreate(0, &wi, GWIN_FRAME_CLOSE_BTN);// | GWIN_FRAME_MINMAX_BTN);

	// Apply some default values for GWIN
	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = true;

	// Apply the list parameters
	wi.g.width = 65;
	wi.g.height = 50;
	wi.g.y = 10;
	wi.g.x = 10;
	wi.text = "Month";
	wi.g.parent = ghFrame1;

	// Create the actual list
	ghListMonth = gwinListCreate(NULL, &wi, FALSE);
	char item[20];
	int i = 0;
	for (i = 0; i < 12; i++) {
		sprintf(item, "%s", months[i]);
		gwinListAddItem(ghListMonth, item, true);
	}
	gwinListSetScroll(ghListMonth, scrollSmooth);

	// Apply some default values for GWIN
	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = true;

	// Apply the list parameters
	wi.g.width = 25;
	wi.g.height = 50;
	wi.g.y = 10;
	wi.g.x = 80;
	wi.text = "Day";
	wi.g.parent = ghFrame1;

	// Create the actual list
	ghListDay = gwinListCreate(NULL, &wi, FALSE);
	for (i = 1; i <= 31; i++) {
		sprintf(item, "%i", i);
		gwinListAddItem(ghListDay, item, true);
	}
	gwinListSetScroll(ghListDay, scrollSmooth);

	// Apply some default values for GWIN
	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = true;

	// Apply the list parameters
	wi.g.width = 45;
	wi.g.height = 50;
	wi.g.y = 10;
	wi.g.x = 110;
	wi.text = "Year";
	wi.g.parent = ghFrame1;

	// Create the actual list
	ghListYear = gwinListCreate(NULL, &wi, FALSE);
	for (i = 2016; i <= 2030; i++) {
		sprintf(item, "%i", i);
		gwinListAddItem(ghListYear, item, true);
	}
	gwinListSetScroll(ghListYear, scrollSmooth);

	wi.g.width = 25;
	wi.g.height = 50;
	wi.g.y = 10;
	wi.g.x = 160;
	wi.text = "Hour";
	wi.g.parent = ghFrame1;

	// Create the actual list
	ghListHour = gwinListCreate(NULL, &wi, FALSE);
	for (i = 1; i <= 24; i++) {
		sprintf(item, "%i", i);
		gwinListAddItem(ghListHour, item, true);
	}
	gwinListSetScroll(ghListHour, scrollSmooth);

	wi.g.width = 25;
	wi.g.height = 50;
	wi.g.y = 10;
	wi.g.x = 190;
	wi.text = "Min";
	wi.g.parent = ghFrame1;

	// Create the actual list
	ghListMin = gwinListCreate(NULL, &wi, FALSE);
	for (i = 1; i <= 60; i++) {
		sprintf(item, "%i", i);
		gwinListAddItem(ghListMin, item, true);
	}
	gwinListSetScroll(ghListMin, scrollSmooth);

	// create Slider1
	wi.g.y = 10; wi.g.x = 235; wi.g.width = 20; wi.g.height = 100; wi.text = "B";
	wi.g.parent = ghFrame1;
	ghSliderBl = gwinSliderCreate(NULL, &wi);

	// create Slider2
	wi.g.y = 10; wi.g.x = 260; wi.g.width = 20; wi.g.height = 100; wi.text = "C";
	wi.g.parent = ghFrame1;
	ghSliderCt = gwinSliderCreate(NULL, &wi);

	// Apply the checkbox parameters
	wi.g.width = 60;        // includes text
	wi.g.height = 20;
	wi.g.y = 75;
	wi.g.x = 10;
	wi.text = " 12 Hr";
	ghCheckBoxHR = gwinCheckboxCreate(NULL, &wi);

	// create button widget: ghBtnSettings
	wi.g.show = TRUE;
	wi.g.x = 235;
	wi.g.y = 130;
	wi.g.width = 50;
	wi.g.height = 27;
	wi.g.parent = ghFrame1;
	wi.text = "Save";
	wi.customDraw = gwinButtonDraw_Rounded;
	//wi.customParam = &iconSettings;
	//wi.customStyle = &WhiteWidgetStyle;
	ghBtnSave = gwinButtonCreate(0, &wi);
	gwinSetFont(ghBtnSave, dejavu_sans_16_anti_aliased_u);

	// Create label widget: ghlblDateTime
	wi.g.show = TRUE;
	wi.g.x = 75;
	wi.g.y = 75;
	wi.g.width = 150;
	wi.g.height = 17;
	wi.g.parent = ghFrame1;
	wi.text = "00/00/00 00:00:00";
	wi.customDraw = gwinLabelDrawJustifiedLeft;
	wi.customParam = 0;
	//wi.customStyle = &syBar;
	ghlblDateTime = gwinLabelCreate(0, &wi);
	gwinLabelSetBorder(ghlblDateTime, FALSE);
	gwinSetFont(ghlblDateTime, dejavu_sans_16_anti_aliased_u);
	gwinRedraw(ghlblDateTime);
}

// Draw the complete graph. This is also where we update the ring
// buffer for graph values
void updateGraphData(float reading, uint8_t graph)
{
	// Adjust scale: we have GRAPH_MEMORY points, so whenever
	// we cross a scale boundary, we initiate a countdown timer.
	// This timer goes down at each redraw if we're under the previous lower scale
	// boundary, otherwise it is reset. If we stay below the scale boundary until it
	// reaches zero, then we scale down.

	if (reading > autoscale_limits[graph][graph_MAX[graph]]) {
		// We need to scale up:
		while (reading > autoscale_limits[graph][graph_MAX[graph]]) {
		  graph_MAX[graph]++;
		  if (graph_MAX[graph] == autoscale_size[graph]-1)
			break;
		  }
		autoscale_countdown[graph] = GRAPH_MEMORY;
		autoscale_max_reading[graph] = 0;
		scaleChanged = true;
		// Let user know the values just got rescaled
		//display.setPrintPos(122,7);
		//display.print("*");
		//delay(100);
	} else if (graph_MAX[graph] > 0) {
		// Do we need to scale down ?
		if (reading < autoscale_limits[graph][graph_MAX[graph]-1]) {
			autoscale_countdown[graph]--; // If we are below the lower scale
			// Keep track of max value of reading during the time we're under current
			// scale limit, so that when we scale down, we go to the correct scale
			// value right away:
			if (reading > autoscale_max_reading[graph])
				autoscale_max_reading[graph] = reading;
			if (autoscale_countdown[graph] == 0) {
				// Time to scale down:
				while (autoscale_max_reading[graph] < autoscale_limits[graph][graph_MAX[graph]-1]) {
					graph_MAX[graph]--;
					if (graph_MAX[graph] == 0) {
						break;
					}
				}

				autoscale_countdown[graph] = GRAPH_MEMORY;
				autoscale_max_reading[graph] = 0;
				scaleChanged = true;
				// Let user know the values just got rescaled
				//display.setPrintPos(122,7);
				//display.print("*");
				//delay(100);
			}
		} else {
			autoscale_countdown[graph] = GRAPH_MEMORY; // we are above the scale under us
			autoscale_max_reading[graph] = 0;
		}
	}
	graph_Mem[graph][ring_idx[graph]] = /*(uint16_t)*/ reading;
	ring_idx[graph] = (ring_idx[graph]+1)%GRAPH_MEMORY;
}


int mapI(int x, int in_min, int in_max, int out_min, int out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


void setLabelTime(GHandle gh, bool timeDate)
{
	struct tm timp;
	char str_date[100];

	rtcGetTime(RTC_DRIVER, &timespec);
	rtcConvertDateTimeToStructTm(&timespec, &timp,0);
	if (timeDate) {
		if(use12HR)
			strftime(str_date, sizeof(str_date), "%x-%r", &timp);
		else
			strftime(str_date, sizeof(str_date), "%x-%X", &timp); //%x for date http://www.tutorialspoint.com/c_standard_library/c_function_strftime.htm

		//chprintf((BaseSequentialStream*)USB_SERIAL_DRIVER, "%s\r\n", str_date);
		gwinSetText(gh, str_date, TRUE);
	} else {
		if(use12HR)
			strftime(str_date, sizeof(str_date), "%r", &timp);
		else
			strftime(str_date, sizeof(str_date), "%X", &timp);

		gwinSetText(gh, str_date, TRUE);
	}
}

void setStartTime(void)
{
	rtcGetTime(RTC_DRIVER, &timespec);
	rtcConvertDateTimeToStructTm(&timespec, &startTime,0);
}

void drawGraphLines(GHandle gh)
{
	 //color_t oldColor;
	 //oldColor = gh->color;
//     gh->color=White;
//     //Vertical Line
//     gwinDrawLine(gh, 5,0, 5,gwinGetHeight(gh));
//     gwinDrawLine(gh, 0,(gwinGetHeight(gh)/2)-5, 5,(gwinGetHeight(gh)/2)-5); //half way tick
//     gwinDrawLine(gh, 3,(gwinGetHeight(gh)/4)-5, 5,(gwinGetHeight(gh)/4)-5); //qtr ticks
//     gwinDrawLine(gh, 3,((gwinGetHeight(gh)/4)*3)-5, 5,((gwinGetHeight(gh)/4)*3)-5); //qtr ticks
//     gwinDrawLine(gh, 0,0, 5,0); //top end
//     //Horizontal Line
//     gwinDrawLine(gh, 0,gwinGetHeight(gh)-5, gwinGetWidth(gh),gwinGetHeight(gh)-5);
//     gwinDrawLine(gh, (gwinGetWidth(gh)/2)+5, gwinGetHeight(gh)-5, (gwinGetWidth(gh)/2)+5, gwinGetHeight(gh)); //half way tickhttps://www.google.com/webhp?hl=en
//     gwinDrawLine(gh, (gwinGetWidth(gh)/4)+5, gwinGetHeight(gh)-5, (gwinGetWidth(gh)/4)+5, gwinGetHeight(gh)-3); //qtr tick
//     gwinDrawLine(gh, ((gwinGetWidth(gh)/4)*3)+5, gwinGetHeight(gh)-5, ((gwinGetWidth(gh)/4)*3)+5, gwinGetHeight(gh)-3); //qtr way tick
//     gwinDrawLine(gh, gwinGetWidth(gh)-1, gwinGetHeight(gh)-5, gwinGetWidth(gh)-1, gwinGetHeight(gh)); //bottom end

	coord_t i, j;
	color_t r,g,b;
	coord_t width = gwinGetWidth(gh);
	coord_t height = gwinGetHeight(gh);

	for (i = 0; i < width; i++) {
	   for(j = 0; j < height; j++) {
		   //r = ((((_acgraphH[i+(j*width)] >> 11) & 0x1F)*527)+23) >> 6;
		   //g = ((((_acgraphH[i+(j*width)] >> 5) & 0x3f)*259)+33) >> 6;
		   //b = (((_acgraphH[i+(j*width)])  & 0x1f) + 23) >> 6;
		   if (activeGraph == 3 || gh->parent == ghContainerGraphCV) {
			   r = RED_OF(_acgraphH2[i+(j*width)] >> 11);
			   g = GREEN_OF(_acgraphH2[i+(j*width)] >> 5);
			   b = BLUE_OF(_acgraphH2[i+(j*width)]);
			   surface2[j*width+i]=(RGB2COLOR(r,g,b));
		   } else {
			   r = RED_OF(_acgraphH[i+(j*width)] >> 11);
			   g = GREEN_OF(_acgraphH[i+(j*width)] >> 5);
			   b = BLUE_OF(_acgraphH[i+(j*width)]);
			   surface[j*width+i]=(RGB2COLOR(r,g,b));
		   }
		   //gwinDrawPixel(pixmap,i, j);
	   }
	}
	//gwinBlitArea(gh, 0,0, width, height, 0,0, width, surface);
	//gh->color=oldColor;
	gwinRedraw(gh);
}

void displayGraph(GHandle gh, uint8_t graph)
{
	coord_t i;
	char str[7];
	color_t r, g, b;

	if (updateGraph) {
		coord_t width = gwinGetWidth(gh);

		for (i = 1; i < GRAPH_MEMORY-7; i++) {
			//uint16_t val = mapI(graph_Mem[(i+ring_idx)%GRAPH_MEMORY], 0, autoscale_limits[graph_MAX], 0, 185);
			uint16_t val = mapf(graph_Mem[graph][(i+ring_idx[graph])%GRAPH_MEMORY], 0, autoscale_limits[graph][graph_MAX[graph]], gwinGetHeight(gh)-6, 0);
			uint16_t pval = mapf(graph_Mem[graph][(i+ring_idx[graph]-1)%GRAPH_MEMORY], 0, autoscale_limits[graph][graph_MAX[graph]], gwinGetHeight(gh)-6, 0);
			//gh->color=Black;
			//gh->color=HTML2COLOR(graphSM[i]);
			//r = ((((_acgraphH[i+6+(pval*width)] >> 11) & 0x1F)*527)+23) >> 6;
			//g = ((((_acgraphH[i+6+(pval*width)] >> 5) & 0x3f)*259)+33) >> 6;
			//b = (((_acgraphH[i+6+(pval*width)])  & 0x1f) + 23) >> 6;
			if (activeGraph == 3) {
				r = RED_OF(_acgraphH2[i+6+(pval*width)] >> 11);
				g = GREEN_OF(_acgraphH2[i+6+(pval*width)] >> 5);
				b = BLUE_OF(_acgraphH2[i+6+(pval*width)]);
				surface2[pval*width+i+6]=RGB2COLOR(r,g,b);
				surface2[val*width+i+6]=Blue;//RGB2COLOR(0,0,255);
			} else {
				r = RED_OF(_acgraphH[i+6+(pval*width)] >> 11);
				g = GREEN_OF(_acgraphH[i+6+(pval*width)] >> 5);
				b = BLUE_OF(_acgraphH[i+6+(pval*width)]);
				surface[pval*width+i+6]=RGB2COLOR(r,g,b);
				surface[val*width+i+6]=Blue;//RGB2COLOR(0,0,255);
			}
			//gh->color=(RGB2COLOR(r,g,b));
			//gwinDrawLine(gh, i+8, 0, i+8, gwinGetHeight(gh)-6);
			//gwinDrawPixel(gh,i+6, pval);

			//gh->color=Blue;
		   // gwinDrawPixel(gh, i+6, val);

		}

		if (activeGraph == 3) {
			snprintf(str, 7, "%d%s", autoscale_limits[1][graph_MAX[1]], units[graphUnits[1]]);
			gwinDrawString(ghContainerGraphCV_2, gwinGetWidth(ghContainerGraphCV_2)-25, 5, str);
		}
		//gwinBlitArea(gh, 0,0, width, height, 0,0, width, surface);
		gwinRedraw(gh);
	}
}

void setActiveGraph(uint8_t graph)
{
	(void)graph;

	if(activeGraph == 0) {
		gwinShow(ghContainerGraphC);
		gwinHide(ghContainerGraphV);
		gwinHide(ghContainerGraphW);
		scaleChanged = true;
		chThdSleepMilliseconds(10);
		drawGraphLines(ghContainerGraphC);
	}

	if(activeGraph == 1) {
		gwinHide(ghContainerGraphC);
		gwinShow(ghContainerGraphV);
		gwinHide(ghContainerGraphW);
		scaleChanged = true;
		chThdSleepMilliseconds(10);
		drawGraphLines(ghContainerGraphV);
	}

	if(activeGraph == 2) {
		gwinHide(ghContainerGraphC);
		gwinHide(ghContainerGraphV);
		gwinShow(ghContainerGraphW);
		scaleChanged = true;
		chThdSleepMilliseconds(10);
		drawGraphLines(ghContainerGraphW);
	}

	if (activeGraph == 3) {
		gwinHide(ghContainerGraphC);
		gwinHide(ghContainerGraphV);
		gwinHide(ghContainerGraphW);
		gwinShow(ghContainerGraphCV);
		scaleChanged = true;
		chThdSleepMilliseconds(10);
		drawGraphLines(ghContainerGraphCV_1);
		drawGraphLines(ghContainerGraphCV_2);
	}
}

void graphInit(void)
{
	pixmap = gdispPixmapCreate(PIXMAP_WIDTH, PIXMAP_HEIGHT);
	gdispImageOpenMemory(&graphImg, gdispPixmapGetMemoryImage(pixmap));
	surface = gdispPixmapGetBits(pixmap);
	drawGraphLines(ghContainerGraphC);
	gwinSetCustomDraw(ghContainerGraphC, gwinContainerDraw_Image, &graphImg);
	gwinSetCustomDraw(ghContainerGraphV, gwinContainerDraw_Image, &graphImg);
	gwinSetCustomDraw(ghContainerGraphW, gwinContainerDraw_Image, &graphImg);

	//Load 2nd smaller graph same graph used for each half, top and bottom sort of like split screen
	pixmap2 = gdispPixmapCreate(PIXMAP_WIDTH, PIXMAP_HEIGHT/2);
	gdispImageOpenMemory(&graphImg2, gdispPixmapGetMemoryImage(pixmap2));
	surface2 = gdispPixmapGetBits(pixmap2);
	drawGraphLines(ghContainerGraphCV_1);
	gwinSetCustomDraw(ghContainerGraphCV_1, gwinContainerDraw_Image, &graphImg2);
	gwinSetCustomDraw(ghContainerGraphCV_2, gwinContainerDraw_Image, &graphImg2);
}
