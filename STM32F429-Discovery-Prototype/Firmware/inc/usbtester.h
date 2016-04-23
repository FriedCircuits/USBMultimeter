/*
    FriedCircuits.us
    USBTester Config for all devices and settings

*/

#ifndef _USBTESTER_H
#define _USBTESTER_H

#include "ch.h"
#include "hal.h"
#include "gfx.h"
#include "rsc/graph.h"

#define UNIT_V      0
#define UNIT_MA     1
#define UNIT_W      2
#define UNIT_MAH    3
#define UNIT_MWH    4

char *units[5]= {"V","mA","W","mAh","mWh"};

//#define READFREQ    10000 //microseconds
uint32_t    READFREQ=100000;
uint16_t    GRAPH_Refresh=100; //10Hz

//USB Serial
#define USB_SERIAL_DRIVER   &SDU1

//RTC
#define RTC_DRIVER  &RTCD1

//GFX
static GHandle      ghFrame1;
static GHandle      ghListMonth;
static GHandle      ghListDay;
static GHandle      ghListYear;
static GHandle      ghBtnSave;
static GHandle      ghListHour;
static GHandle      ghListMin;
static GHandle      ghSliderBl;
static GHandle      ghSliderCt;
static GHandle      ghCheckBoxHR;
static GHandle      ghlblDateTime;

gdispImage graphImg;
gdispImage graphImg2;

// Fonts
//font_t ugfx_font_2;
//font_t dejavu_sans_12_anti_aliased;
//font_t dejavu_sans_20_anti_aliased;
font_t dejavu_sans_16_anti_aliased_u;

#define PIXMAP_WIDTH	300
#define PIXMAP_HEIGHT	190

static GDisplay* pixmap;
static pixel_t* surface;

static GDisplay* pixmap2;
static pixel_t* surface2;

char *months[12] = {
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December" };

//Graph Ring from Original USB Tester
// Graph values:
// Graph area is from 0 to 127
#define GRAPH_MEMORY 299
//Number of graphs
#define NUM_GRAPHS 3

//Graph history
float graph_Mem[NUM_GRAPHS][GRAPH_MEMORY];
uint16_t ring_idx[NUM_GRAPHS] = {0,0,0}; // graph_Mem is managed as a ring buffer.

// Auto scale management
uint16_t autoscale_limits[NUM_GRAPHS][7] = {{100, 200, 500, 1000, 1500, 2500, 3200},{4,5,9,12,15,20,26}, {1,2,4,6,10,20,40}}; // in mA
uint16_t autoscale_size[NUM_GRAPHS] = {sizeof(autoscale_limits[0]) / sizeof(uint16_t),sizeof(autoscale_limits[1]) / sizeof(uint16_t), sizeof(autoscale_limits[1]) / sizeof(uint16_t) };
uint16_t graph_MAX[NUM_GRAPHS] = {0,0,0}; // Max scale by default
float autoscale_max_reading[NUM_GRAPHS] = {0,0,0};  // The memorized maximum reading over the window
uint16_t autoscale_countdown[NUM_GRAPHS]= {GRAPH_MEMORY, GRAPH_MEMORY, GRAPH_MEMORY};
uint8_t activeGraph = 0; //Startup current active graph
bool scaleChanged = false;
uint8_t graphUnits[NUM_GRAPHS] = {UNIT_MA, UNIT_V, UNIT_W};
bool firstBoot = true;

#endif /* _USBTESTER_H */
