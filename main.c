/*
 *  ======== main.c ========
 // Veli-Matti Veijola / veldu.veijola(at)outlook.com / 2466639
 // Mikael Malmi / mikael.malmi@outlook.com / 2520902
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

//Include omat
#include <math.h>

/* JTKJ Header files */
#include "wireless/comm_lib.h"

/* Task Stacks */
#define STACKSIZE 2048
Char labTaskStack[STACKSIZE];
Char commTaskStack[STACKSIZE];

//OMAT GLOBAALIT
float datalista[60][6] = {}; 
float summat[6];
float varianssit[6];
enum state { idle=0, lampotila=1, viesti=2, exit=3, mittaus=4, lampotila_mittaus=5, lue_viesti=6, exit_state1=7, exit_state2=8 };
enum state mystate = idle;
double pressure;
double temperature;
int viesti_tila = 0;
char payload1[16] = "e";
int last_drawn; //Viimeksi piirrettyn grafiikan "id"/tunniste
int viesti_vastaanotto = 0;

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

//OMAT FUNKTIOT
void sensori_main(lista);

float keskiarvo(float data[][6], int indeksi);

void grafiikka_piirto(graphics_id);

void animated_graphics(graphic_id);

void tyhjenna_naytto();

void datankerays();

void paattely();

void update_screen();

void clkFxn(UArg arg0);

/* JTKJ: Handle for power button */
void powerButtonFxn(PIN_Handle handle, PIN_Id pinId) {
    // Vaihdetaan led-pinnin tilaa negaatiolla
    PIN_setOutputValue( hLed, Board_LED0, !PIN_getOutputValue( Board_LED0 ) );
     
   // Tilojen vaihto alemmasta napista menussa
   //0-3 tilat päämenussa
   	if (mystate == 0) {
   	    mystate = 1;
   	} else if (mystate == 1) {
   	    mystate = 2;
   	} else if (mystate == 2) {
        mystate = 3;
   	} else if (mystate == 3) {
   	    mystate = 0;
   	//7-8 tilat exit-menussa    
   	} else if (mystate == 7) {
   	    mystate = 8;
   	} else if (mystate == 8) {
   	    mystate = 7;
   	}
}

/* JTKJ: WRITE HERE YOUR HANDLER FOR BUTTON0 PRESS */
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    //Ylempi nappi toteuttaa toiminnallisuudet
    // Vaihtaa tilan idlesta mittaukseen
    if (mystate == 0) {
        mystate = 4;
    } else if (mystate == 1) {
   	    // Tilan vaihto menussa Lämpötila -> Lämpötila nayttö
        mystate = 5;
        // Vaihdetaan led-pinnin tilaa negaatiolla
        PIN_setOutputValue( hLed, Board_LED0, !PIN_getOutputValue( Board_LED0 ) );
        
    } else if (mystate == 2) {
        // Tilan vaihto menussa Viestit -> Viestin nayttö
        mystate = 6;
        
    } else if (mystate == 3) {
        // Tilan vaihto, EXIT -> Exit vaihtoehdot
        mystate = 7;
    
    } else if (mystate == 7) {
        Display_clear(hDisplay);
        Task_sleep(100000 / Clock_tickPeriod);

	    PIN_close(hPowerButton);

        PINCC26XX_setWakeup(cPowerWake);
	    Power_shutdown(NULL,0);

    } else if (mystate == 8) {
        // Palauttaa tilan takaisin menuun
        mystate = 0;
    }
}

/* JTKJ: Communication Task */
Void commTask(UArg arg0, UArg arg1) {
    char payload[16];
    uint16_t senderAddr;
    
    // Radio to receive mode
    int32_t result = StartReceive6LoWPAN();
    if(result != true) {
      System_abort("Wireless receive start failed");
    }

    // Aina lähetyksen jälkeen pitää palata vastaanottotilaan
    StartReceive6LoWPAN();
    
    while (1) {
        //Viestin lähetys päättelyfunktion tuloksen mukaan
        if (viesti_tila == 1) {
            sprintf(payload,"11 Meni portaita");
            Send6LoWPAN(IEEE80154_SERVER_ADDR, payload, strlen(payload));
            System_printf("Viesti lahetetty\n");
            System_flush();
            viesti_tila = 0;
        }
        else if (viesti_tila == 2) {
            sprintf(payload,"11 Meni hissilla");
            Send6LoWPAN(IEEE80154_SERVER_ADDR, payload, strlen(payload));
            System_printf("Viesti lahetetty\n");
            System_flush();
            viesti_tila = 0;
        }

        // jos true, viesti odottaa
        if (GetRXFlag()) {

            // Tyhjennetään puskuri (ettei sinne jäänyt edellisen viestin jämiä)
            memset(payload1,0,16);
            // Luetaan viesti puskuriin payload
            Receive6LoWPAN(&senderAddr, payload1, 16);
            viesti_vastaanotto = 1;
            
            //Tulostetaan vastaanotettu viesti konsoli-ikkunaan
            System_printf(payload1);
            System_printf("\n");
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
    
    // JTKJ: Init Display 
    Display_Params displayParams;
	displayParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&displayParams);

    hDisplay = Display_open(Display_Type_LCD, &displayParams);
    if (!hDisplay) {
        System_abort("Naytto ei toimi");
    }
    Display_clear(hDisplay);
    
    if (hDisplay) {
      
      tContext *pContext = DisplayExt_getGrlibContext(hDisplay);

      if (pContext) {
        // Latausruudun aloitus grafiikka
        grafiikka_piirto(1);
         // Piirto puskurista näytölle
         GrFlush(pContext);
        }
    }

    Task_sleep(3000000 / Clock_tickPeriod); //3s tauko
    
    char str[50];
    sprintf(str,"%4x", IEEE80154_MY_ADDR);
    Display_print0(hDisplay, 5, 5, str); 
    
    //Tyhjennä näyttö
    Display_clear(hDisplay);

    
        i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
	    if (i2cMPU == NULL) {
	        System_abort("Error Initializing I2CMPU\n");
	    }

    // JTKJ: main loop
    while (1) {

        switch (mystate) {
            case 0:
                // MENU 0 , Valikko kohta "Mittaustila"
                update_screen();
                break;

            case 1:
                //MENU 1, Valikko kohta "Lampotila"
                update_screen();
                break;
                
            case 2:
                //MENU 2, Valikko kohta "Viestit"
                update_screen();
                break;
                
            case 3:
                //MENU 3, Valikko kohta "EXIT"
                update_screen();
                break;
                
            case 4:
                //MITTAUSTILA, suorituksen jälkeen palaa MENU 0
                update_screen();
                sensori_main(datalista); 
                mystate = 0;
                break;
            
            case 5:
                //LAMPÖTILA-MITTAUS
                Display_clear(hDisplay);
                I2C_close(i2cMPU); // Close MPU for BMP
    	
            	i2c = I2C_open(Board_I2C, &i2cParams);
        	    if (i2c == NULL) {
        	        System_abort("Error Initializing I2C\n");
        	    }
            	bmp280_get_data(&i2c, &pressure, &temperature);
                
                I2C_close(i2c); //Close BMP for MPU
                
                i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
	            if (i2cMPU == NULL) {
	                System_abort("Error Initializing I2CMPU\n");
	            }
	            
	            update_screen();
	            Task_sleep(4000000 / Clock_tickPeriod); //Näytetään lämpötila 4s
	            Display_clear(hDisplay);
	            mystate = 1;
                break;
            
            case 6:
                //viestin luku
                update_screen();
                Task_sleep(4000000 / Clock_tickPeriod); //näytetään 4s
                Display_clear(hDisplay); 
                mystate = 2;
                
            case 7:
                //exit Menu, "KYLLÄ" vaihtoehto
                update_screen();
                break;
                
            case 8:
                //exit Menu, "EI" vaihtoehto
                update_screen();
                break;
        }
        
        
    	// JTKJ: Do not remove sleep-call from here!
    	Task_sleep(400000 / Clock_tickPeriod);
    	// Display_clear(hDisplay);
    }
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
}


// Taskifunktio
Int main(void) {

    // Task variables
	Task_Handle hLabTask;
	Task_Params labTaskParams;
	Task_Handle hCommTask;
	Task_Params commTaskParams;
	Clock_Handle clkHandle;
    Clock_Params clkParams;

    // Initialize board
    Board_initGeneral();
    Board_initI2C();
    
	// Initialize clock
	Clock_Params_init(&clkParams);
    clkParams.period = 7000000 / Clock_tickPeriod; 
    clkParams.startFlag = TRUE;
    
    clkHandle = Clock_create((Clock_FuncPtr)clkFxn, 1000000 / Clock_tickPeriod, &clkParams, NULL);
    if (clkHandle == NULL) {
       System_abort("Clock creat failed");
    }
    
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

//OMAT FUNKTIOT

void grafiikka_piirto(graphics_id) { // Piirtaa tarvittavan grafiikan, jos kyseista grafiikkaa ei ole piirretty viime kerralla.
    tContext *pContext = DisplayExt_getGrlibContext(hDisplay);
    if (graphics_id != last_drawn) {
        if (graphics_id == 1) { // Load screen graphic [1]
            GrLineDraw(pContext,0,4,54,4);
            GrLineDraw(pContext,54,4,60,1);
            GrLineDraw(pContext,60,1,96,1);
            GrLineDraw(pContext,0,92,91,92);
            GrLineDraw(pContext,91,92,96,87); 
            
        } else if (graphics_id == 2) { // Idle/mittaus Screen graphic [2]
            GrLineDraw(pContext,0,4,54,4);
            GrLineDraw(pContext,54,4,60,1);
            GrLineDraw(pContext,60,1,96,1);
            GrLineDraw(pContext,0,92,91,92);
            GrLineDraw(pContext,91,92,96,87); 

        } else if (graphics_id == 3) { // Non-idle Screen graphic [3]
            GrLineDraw(pContext,0,4,54,4);
            GrLineDraw(pContext,54,4,60,1);
            GrLineDraw(pContext,60,1,96,1);
            GrLineDraw(pContext,0,92,91,92);
            GrLineDraw(pContext,91,92,96,87); 
        } else if (graphics_id == 4) { // EXIT menu graphic [4]
            GrLineDraw(pContext,0,4,54,4);
            GrLineDraw(pContext,54,4,60,1);
            GrLineDraw(pContext,60,1,96,1);
            GrLineDraw(pContext,0,92,91,92);
            GrLineDraw(pContext,91,92,96,87); 
        } 
        last_drawn = graphics_id;
        GrFlush(pContext);
    }
}
void tyhjenna_naytto() { // Tyhjentaa aiemman tekstin naytosta, mutta ei taustagrafiikkaa
    Display_print0(hDisplay, 6, 1, " ");
    Display_print0(hDisplay, 7, 1, " ");
    Display_print0(hDisplay, 8, 1, " ");
    Display_print0(hDisplay, 9, 1, " ");
    Display_print0(hDisplay, 10, 1, " ");
}

void animated_graphics(graphic_id) { // Hoitaa animoidun tekstin tulostuksen
    tContext *pContext = DisplayExt_getGrlibContext(hDisplay);
    if (graphic_id == 1) {
        
        int x = 44;
        int y = 64;
        int i;
        
        for (i=0; i<8; i++) {
        y = y + 1;
        tyhjenna_naytto(); 
        
        Display_print0(hDisplay, 3, 0, "KULJIT HISSILLA");
        Display_print0(hDisplay, 5, 7, ":(");

        // Tikku-ukko 0
        GrLineDraw(pContext,3+x,0+y,4+x,1+y);
        GrLineDraw(pContext,4+x,2+y,3+x,2+y);
        GrLineDraw(pContext,2+x,1+y,2+x,2+y);
        GrLineDraw(pContext,3+x,3+y,3+x,8+y);
        GrLineDraw(pContext,2+x,4+y,4+x,4+y);
        GrLineDraw(pContext,1+x,5+y,1+x,6+y);
        GrLineDraw(pContext,5+x,5+y,5+x,6+y);
        GrLineDraw(pContext,4+x,8+y,4+x,10+y);
        GrLineDraw(pContext,2+x,8+y,2+x,10+y); 
        GrLineDraw(pContext,1+x,11+y,2+x,11+y); 
        GrLineDraw(pContext,4+x,11+y,5+x,11+y); 
 
        //Hissi osio
        GrLineDraw(pContext,-2+x,-2+y,8+x,-2+y); 
        GrLineDraw(pContext,-2+x,-2+y,-2+x,-1+y); 
        GrLineDraw(pContext,8+x,-2+y,8+x,-1+y); 
        
        GrLineDraw(pContext,-2+x,13+y,8+x,13+y); 
        GrLineDraw(pContext,-2+x,13+y,-2+x,12+y); 
        GrLineDraw(pContext,8+x,12+y,8+x,12+y); 
        
        GrFlush(pContext);
        Task_sleep(1 * 500000/Clock_tickPeriod); // Animaation frame toistonopeus 0,5s = 2fps
        }
    } else if (graphic_id == 2) {
        int x = 44;
        int y = 64;
        int i;
        int o;
        
        for (o=0; o<8; o++) {
        if (o % 2 == 0) {
            i = 0;
        } else {
            i = 1;
        }
        
        tyhjenna_naytto();
        Display_print0(hDisplay, 3, 0, "KULJIT PORTAITA");
        Display_print0(hDisplay, 5, 7, ":)");

        if (i == 0) {
            // Tikku-ukko 1
            GrLineDraw(pContext,6+x,-4+y,4+x,-4+y);
            GrLineDraw(pContext,5+x,-3+y,5+x,-2+y);
            GrLineDraw(pContext,6+x,-3+y,7+x,-2+y);
            GrLineDraw(pContext,6+x,-2+y,6+x,3+y);
            GrLineDraw(pContext,4+x,1+y,4+x,2+y);
            GrLineDraw(pContext,4+x,0+y,5+x,0+y); 
            GrLineDraw(pContext,7+x,0+y,8+x,-1+y); 
            GrLineDraw(pContext,9+x,-3+y,9+x,-2+y); 
            GrLineDraw(pContext,5+x,3+y,5+x,5+y);
            GrLineDraw(pContext,4+x,5+y,4+x,6+y); 
            GrLineDraw(pContext,3+x,6+y,3+x,7+y);
            GrLineDraw(pContext,7+x,3+y,9+x,5+y); 
            GrLineDraw(pContext,7+x,4+y,8+x,5+y); 
            
        } else if (i = 1) {
            //Tikku-ukko 2
            GrLineDraw(pContext,4+x,-1+y,2+x,0+y);
            GrLineDraw(pContext,5+x,-1+y,3+x,1+y);
            GrLineDraw(pContext,5+x,1+y,4+x,2+y);
            GrLineDraw(pContext,1+x,3+y,7+x,3+y);
            GrLineDraw(pContext,4+x,4+y,4+x,6+y);
            GrLineDraw(pContext,3+x,7+y,3+x,8+y);
            GrLineDraw(pContext,2+x,8+y,2+x,9+y);
            GrLineDraw(pContext,1+x,9+y,1+x,10+y);
            GrLineDraw(pContext,5+x,7+y,5+x,8+y); 
            GrLineDraw(pContext,5+x,9+y,6+x,9+y); 
        }
         
        GrLineDraw(pContext,-1+x,14+y,11+x,2+y); 
        // Portaat
        GrLineDraw(pContext,0+x,12+y,0+x,14+y); 
        GrLineDraw(pContext,2+x,10+y,2+x,12+y); 
        GrLineDraw(pContext,4+x,8+y,4+x,10+y); 
        GrLineDraw(pContext,6+x,6+y,6+x,8+y); 
        GrLineDraw(pContext,8+x,4+y,8+x,6+y); 
        GrLineDraw(pContext,10+x,2+y,10+x,4+y); 
        GrLineDraw(pContext,12+x,0+y,12+x,2+y);
        
        GrFlush(pContext);
        Task_sleep(1 * 500000/Clock_tickPeriod); // Animaation frame toistonopeus 0,5s = 2fps
        
        }
    }
}

void sensori_main(lista) { // Liikkumisen tunnistuksen pääfunktio
    int j;
    char tulostus[50];
    char tulostus2[50];
    
    datankerays();
    System_printf("Datan kerays ok\n");
	System_flush();
    
    for (j=0; j<6; j++) {
        keskiarvo(lista, j); 
    }
    sprintf(tulostus,"Variansseja: %.4f, %.4f, %.4f \n", varianssit[0], varianssit[1], varianssit[2]);
    System_printf(tulostus);
    sprintf(tulostus2,"X, Y ja Z keskiarvot: %.2f, %.2f %.2f \n", summat[0], summat[1], summat[2]);
    System_printf(tulostus2);
    System_flush();
    
    paattely();
}

float keskiarvo(float data[][6], int indeksi) { // Laskee keskiarvoja ja variansseja päättely-funktiota varten
    int i;
    int j;
    float summa = 0;
    float var_summa = 0;
    float varianssi = 0;
    int n = 0;
    
    for (i=0; i<60; i++) {
        summa += data[i][indeksi];
        n += 1;
    }
    summa = summa / n;
   
    for (j=0; j<n+1; j++) {
        var_summa += pow((data[j][indeksi] - summa), 2);
    }
    varianssi = var_summa / (n - 1);    
    summat[indeksi] = summa; //SIJOITUKSET GLOBAALEIHIN
    varianssit[indeksi] = varianssi;  
    
    return 0;
}

void datankerays() { // Kerää dataa sensorilta, tallentaa listaan käytettäväksi
    I2C_Handle i2cMPU; // INTERFACE FOR MPU9250 SENSOR
	I2C_Params i2cMPUParams;
	float ax, ay, az, gx, gy, gz;
    int i;
    char tulostus3[10];
	for (i=0; i<60; i++) {
        mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
        datalista[i][0] = ax;
        datalista[i][1] = ay;
        datalista[i][2] = az;
        datalista[i][3] = gx;
        datalista[i][4] = gy;
        datalista[i][5] = gz;
        Task_sleep(1 * 100000/Clock_tickPeriod); //Datan keräys 0.1s välein
    }
}


void paattely() { // Päättelee ollaanko portaissa vai hississä
    int e;
    if (varianssit[2] > 0.05) {
        System_printf("Portaissa \n");
	    System_printf("Viestitila muutettu 1\n");
	    System_flush();
	    viesti_tila = 1;
	    Display_clear(hDisplay);
	    Display_print0(hDisplay, 3, 0, "KULJIT PORTAITA");
        Display_print0(hDisplay, 5, 7, ":)");
        grafiikka_piirto(3);
        animated_graphics(2);
        Task_sleep(1 * 1000000/Clock_tickPeriod);
	    Display_clear(hDisplay);
    }
    for (e=0; e<60; e++) {
        if ((varianssit[2] < 0.05) && (datalista[e][2] < -1.15)) {
            System_printf("Hississa \n");
	        System_printf("Viestitila muutettu 2\n");
	        System_flush();
	        viesti_tila = 2;
	        Display_clear(hDisplay);
	        Display_print0(hDisplay, 3, 0, "KULJIT HISSILLA");
            Display_print0(hDisplay, 5, 7, ":(");
            grafiikka_piirto(3);
            animated_graphics(1);
	        Task_sleep(1 * 1000000/Clock_tickPeriod);
	        Display_clear(hDisplay);
	        
	        break;
        }
        else if ((varianssit[2] < 0.05) && (datalista[e][2] > -0.85)) {
            System_printf("Hississa \n");
	        System_printf("Viestitila muutettu 2\n");
	        System_flush();
	        viesti_tila = 2;
	        Display_clear(hDisplay);
	        Display_print0(hDisplay, 3, 0, "KULJIT HISSILLA");
            Display_print0(hDisplay, 5, 7, ":(");
            grafiikka_piirto(3);
            animated_graphics(1);
	        Task_sleep(1 * 1000000/Clock_tickPeriod);
	        Display_clear(hDisplay);
	        break;
        }
    }
}
    

void update_screen() { // Paivittaa nayton
    if (mystate == 0) {
        grafiikka_piirto(2);
        Display_print0(hDisplay, 1, 1, "Idle");
        Display_print0(hDisplay, 3, 1, "o MITTAUSTILA");
        Display_print0(hDisplay, 5, 3, "LAMPOTILA");
        Display_print0(hDisplay, 7, 3, "VIESTIT");
        Display_print0(hDisplay, 9, 3, "EXIT");
        
    } else if (mystate == 1) {
        grafiikka_piirto(2);
        Display_print0(hDisplay, 1, 1, "Idle");
        Display_print0(hDisplay, 3, 3, "MITTAUSTILA");
        Display_print0(hDisplay, 5, 1, "o LAMPOTILA");
        Display_print0(hDisplay, 7, 3, "VIESTIT");
        Display_print0(hDisplay, 9, 3, "EXIT");
        
    } else if (mystate == 2) {
        grafiikka_piirto(2);        
        Display_print0(hDisplay, 1, 1, "Idle");
        Display_print0(hDisplay, 3, 3, "MITTAUSTILA");
        Display_print0(hDisplay, 5, 3, "LAMPOTILA");
        Display_print0(hDisplay, 7, 1, "o VIESTIT");
        Display_print0(hDisplay, 9, 3, "EXIT");
        
    } else if (mystate == 3) {
        grafiikka_piirto(2);        
        Display_print0(hDisplay, 1, 1, "Idle");
        Display_print0(hDisplay, 3, 3, "MITTAUSTILA");
        Display_print0(hDisplay, 5, 3, "LAMPOTILA");
        Display_print0(hDisplay, 7, 3, "VIESTIT");
        Display_print0(hDisplay, 9, 1, "o EXIT");
        
    } else if (mystate == 4) {
        grafiikka_piirto(2);
        Display_print0(hDisplay, 1, 1, "Mittaa..."); 
        Display_print0(hDisplay, 3, 3, "MITTAUSTILA");
        Display_print0(hDisplay, 5, 3, "LAMPOTILA");
        Display_print0(hDisplay, 7, 3, "VIESTIT");
        Display_print0(hDisplay, 9, 3, "EXIT");
        
    } else if (mystate == 5) {
        Display_clear(hDisplay);
        char str2[20];
        sprintf(str2,"%.1f C", temperature);
        Display_print0(hDisplay, 1, 1, "Lampotila:");
        Display_print0(hDisplay, 3, 1, str2);
        
        char str3[20];
        sprintf(str3,"%.0f Pa", pressure);
        Display_print0(hDisplay, 6, 1, "Ilmanpaine:");
        Display_print0(hDisplay, 8, 1, str3);
        grafiikka_piirto(3);
    } 
    else if (mystate == 6) { 
        Display_clear(hDisplay);
        if (viesti_vastaanotto == 0) {
            Display_print0(hDisplay, 3, 0, "EI VASTAANOTETTU");
            Display_print0(hDisplay, 5, 3, "VIESTEJA");
            grafiikka_piirto(3);
        }
        else if (viesti_vastaanotto == 1) {
            Display_print0(hDisplay, 5, 0, payload1);
            viesti_vastaanotto = 0;
            grafiikka_piirto(3);
        }
        
    } else if (mystate == 7) {
        // Tyhjentää ruudun aiemmasta tekstistä
        Display_print0(hDisplay, 1, 1, " ");
        Display_print0(hDisplay, 7, 3, " ");
        Display_print0(hDisplay, 9, 3, " ");
        
        grafiikka_piirto(4);
        Display_print0(hDisplay, 3, 2, "EXIT?");
        Display_print0(hDisplay, 5, 1, "o KYLLA   EI");
        
        
        
    } else if (mystate == 8) {
        // Tyhjentää ruudun aiemmasta tekstistä
        Display_print0(hDisplay, 1, 1, " ");
        Display_print0(hDisplay, 7, 3, " ");
        Display_print0(hDisplay, 9, 3, " ");
        
        grafiikka_piirto(4);
        Display_print0(hDisplay, 3, 2, "EXIT?");
        Display_print0(hDisplay, 5, 3, "KYLLA o EI");
        
    
    }
}

void clkFxn(UArg arg0) {
    // Muutetaan tilaa halutuksi
    // Huom! If-lauseella asetetaan, että tilasiirto on mahdollinen 
    // vain jos nykyinen tila on IDLE!
    
    //Kellokeskeytys jatetty tarpeettomana tyhjäksi
}