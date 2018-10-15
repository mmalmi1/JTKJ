/*
 *  ======== main.c ========
 */
/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/pin/PINCC26XX.h>


/* TI-RTOS Header files */
#include <ti/drivers/I2C.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>

/* Board Header files */
#include "Board.h"
#include "sensors/mpu9250.h"
#include "sensors/bmp280.h"

/* JTKJ Header files */
#include "wireless/comm_lib.h"

/* Task Stacks */
#define STACKSIZE 2048
Char labTaskStack[STACKSIZE];
Char commTaskStack[STACKSIZE];

/* JTKJ: Display */
Display_Handle hDisplay;

//MPU GLOBAL VARIABLES
static PIN_Handle hMpuPin;
static PIN_State MpuPinState;
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

// MPU9250 uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

/* JTKJ: Pin Button1 configured as power button */
static PIN_Handle hPowerButton;
static PIN_State sPowerButton;
PIN_Config cPowerButton[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};
PIN_Config cPowerWake[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
    PIN_TERMINATE
};

/* JTKJ: Pin Button0 configured as input */
static PIN_Handle hButton0;
static PIN_State sButton0;
PIN_Config cButton0[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE, // JTKJ: CONFIGURE BUTTON 0 AS INPUT (SEE LECTURE MATERIAL)
    PIN_TERMINATE
};

/* JTKJ: Leds */
static PIN_Handle hLed;
static PIN_State sLed;
PIN_Config cLed[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, // JTKJ: CONFIGURE LEDS AS OUTPUT (SEE LECTURE MATERIAL)
    PIN_TERMINATE
};

/* JTKJ: Handle for power button */
void powerButtonFxn(PIN_Handle handle, PIN_Id pinId) {

    char payload[16];
    // Vaihdetaan led-pinnin tilaa negaatiolla
    PIN_setOutputValue( hLed, Board_LED0, !PIN_getOutputValue( Board_LED0 ) );
   
   	sprintf(payload,"I am %x", IEEE80154_MY_ADDR); 
    Send6LoWPAN(IEEE80154_SERVER_ADDR, payload, strlen(payload));

}

/* JTKJ: WRITE HERE YOUR HANDLER FOR BUTTON0 PRESS */
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

   // Vaihdetaan led-pinnin tilaa negaatiolla
   PIN_setOutputValue( hLed, Board_LED0, !PIN_getOutputValue( Board_LED0 ) );
}

/* JTKJ: Communication Task */
Void commTask(UArg arg0, UArg arg1) {
    char payload[16];
    // Radio to receive mode
    int32_t result = StartReceive6LoWPAN();
    if(result != true) {
      System_abort("Wireless receive start failed");
    }

    // Aina lähetyksen jälkeen pitää palata vastaanottotilaan
    StartReceive6LoWPAN();
    
    while (1) {

        // TÄNNE EI VIESTIN LÄHETYSTÄ

        // jos true, viesti odottaa
        if (GetRXFlag()) {

            // Tyhjennetään puskuri (ettei sinne jäänyt edellisen viestin jämiä)
            memset(payload,0,16);
            // Luetaan viesti puskuriin payload
            Receive6LoWPAN(IEEE80154_SERVER_ADDR, payload, 16);
            // Tulostetaan vastaanotettu viesti konsoli-ikkunaan
            System_printf(payload);
            System_flush();
        }
}
        // DO __NOT__ PUT YOUR SEND MESSAGE FUNCTION CALL HERE!! 

    	// NOTE THAT COMMUNICATION WHILE LOOP DOES NOT NEED Task_sleep
    	// It has lower priority than main loop (set by course staff)
}        

/* JTKJ: laboratory exercise task */
Void labTask(UArg arg0, UArg arg1) {

    I2C_Handle i2c; // INTERFACE FOR OTHER SENSORS
    I2C_Params i2cParams;
    I2C_Handle i2cMPU; // INTERFACE FOR MPU9250 SENSOR
	I2C_Params i2cMPUParams;
    
    double temperature;
    double pressure;
    float ax, ay, az, gx, gy, gz;
    char tulostettava[50];

    //jtkj: Create I2C for usage 
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }
    //MPU PWER ON
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // WAIT 100MS FOR THE SENSOR TO POWER UP
	Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();
    
    
    // MPU9250 SETUP AND CALIBRATION
        System_printf("MPU9250: Setup and calibration...\n");
    	System_flush();
    
    	mpu9250_setup(&i2cMPU);
    
    	System_printf("MPU9250: Setup and calibration OK\n");
    	System_flush();
	
	I2C_close(i2cMPU); //CLOSE MPU
	
	
	i2c = I2C_open(Board_I2C0, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }
    
    // JTKJ: SETUP BMP280 SENSOR HERE
    bmp280_setup(&i2c);

    Task_sleep(1 * 1000000/Clock_tickPeriod); 
    
    I2C_close(i2c);
    
    //Näytä teksti ruudulla 1s ajan
    Task_sleep(1 * 1000000/Clock_tickPeriod); 
    
    // JTKJ: Init Display 
    Display_Params displayParams;
	displayParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&displayParams);

    hDisplay = Display_open(Display_Type_LCD, &displayParams);
    if (!hDisplay) {
        System_abort("Naytto ei toimi");
    }
    Display_clear(hDisplay);

    char str[50];
    sprintf(str,"%4x", IEEE80154_MY_ADDR);
    Display_print0(hDisplay, 5, 5, str); 
    
    //Tyhjennä näyttö
    Display_clear(hDisplay);

    /*Display_clear(hDisplay);
    Display_print0(hDisplay, 5, 1, "THE NUMBERS");
    Display_print0(hDisplay, 7, 4, "MASON");*/
    
        i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
	    if (i2cMPU == NULL) {
	        System_abort("Error Initializing I2CMPU\n");
	    }

    // JTKJ: main loop
    while (1) {

    	// JTKJ: MAYBE READ BMP280 SENSOR DATA HERE?
    	/*
    	i2c = I2C_open(Board_I2C, &i2cParams);
	    if (i2c == NULL) {
	        System_abort("Error Initializing I2C\n");
	    }
    	
    	bmp280_get_data(&i2c, &pressure, &temperature);
        sprintf(tulostettava,"Lämpötila on %.1f C ja paine on %.0f Pascalia. \n", temperature, pressure);
    	System_printf(tulostettava);
        System_flush();
        
        char str2[50];
        sprintf(str2,"%.1f C", temperature);
        Display_print0(hDisplay, 1, 1, "Lampotila:");
        Display_print0(hDisplay, 3, 1, str2);
        
        char str3[50];
        sprintf(str3,"%.0f Pa", pressure);
        Display_print0(hDisplay, 6, 1, "Ilmanpaine:");
        Display_print0(hDisplay, 8, 1, str3);
        
        I2C_close(i2c);
        */
        
	    mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
	    sprintf(tulostettava,"Kiih X: %.1f Y: %.1f Z: %.1f \n Gyro X: %.1f Y: %.1f Z: %.1f \n", ax, ay, az, gx, gy, gz);
	    
    	System_printf(tulostettava);
    	System_flush();
        
        // I2C_close(i2cMPU);
        
    	// JTKJ: Do not remove sleep-call from here!
    	Task_sleep(1000000 / Clock_tickPeriod);
    	// Display_clear(hDisplay);
    }
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
}



 Int main(void) {

    // Task variables
	Task_Handle hLabTask;
	Task_Params labTaskParams;
	Task_Handle hCommTask;
	Task_Params commTaskParams;

    // Initialize board
    Board_initGeneral();
    Board_initI2C();

	/* JTKJ: Power Button */
	hPowerButton = PIN_open(&sPowerButton, cPowerButton);
	if(!hPowerButton) {
		System_abort("Error initializing power button shut pins\n");
	}
	if (PIN_registerIntCb(hPowerButton, &powerButtonFxn) != 0) {
		System_abort("Error registering power button callback function");
	}

    // JTKJ: INITIALIZE BUTTON0 HERE
    hButton0 = PIN_open(&sButton0, cButton0);
	if(!hButton0) {
		System_abort("Error initializing power button shut pins\n");
	}
	if (PIN_registerIntCb(hButton0, &buttonFxn) != 0) {
		System_abort("Error registering power button callback function");
	}

    /* JTKJ: Init Leds */
    hLed = PIN_open(&sLed, cLed);
    if(!hLed) {
        System_abort("Error initializing LED pin\n");
    }
    
    /* MPU POWER PIN */
    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL) {
    	System_abort("Pin open failed!");
    }

    /* JTKJ: Init Main Task */
    Task_Params_init(&labTaskParams);
    labTaskParams.stackSize = STACKSIZE;
    labTaskParams.stack = &labTaskStack;
    labTaskParams.priority=2;

    hLabTask = Task_create(labTask, &labTaskParams, NULL);
    if (hLabTask == NULL) {
    	System_abort("Task create failed!");
    }

    /* JTKJ: Init Communication Task */
    Init6LoWPAN();

    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority=1;
    
    hCommTask = Task_create(commTask, &commTaskParams, NULL);
    if (hCommTask == NULL) {
    	System_abort("Task create failed!");
    }

    // JTKJ: Send hello to console
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}

