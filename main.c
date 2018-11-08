// /*
//  *  ======== main.c ========
//  */
// /* XDCtools Header files */
// #include <xdc/std.h>
// #include <xdc/runtime/System.h>
// #include <ti/sysbios/BIOS.h>
// #include <ti/sysbios/knl/Task.h>
// #include <ti/sysbios/knl/Clock.h>
// #include <ti/drivers/i2c/I2CCC26XX.h>
// #include <ti/drivers/pin/PINCC26XX.h>


// /* TI-RTOS Header files */
// #include <ti/drivers/I2C.h>
// #include <ti/drivers/PIN.h>
// #include <ti/drivers/pin/PINCC26XX.h>
// #include <ti/mw/display/Display.h>
// #include <ti/mw/display/DisplayExt.h>

// /* Board Header files */
// #include "Board.h"
// #include "sensors/mpu9250.h"
// #include "sensors/bmp280.h"

// //Include omat
// #include <math.h>
// #include "Koodia/testdata1.h"
// #include "Koodia/testdata2.h" 
// #include "Koodia/testdata3.h" 

// /* JTKJ Header files */
// #include "wireless/comm_lib.h"

// /* Task Stacks */
// #define STACKSIZE 2048
// Char labTaskStack[STACKSIZE];
// Char commTaskStack[STACKSIZE];

// //OMAT GLOBAALIT
// float datalista[60][6] = {}; 
// float summat[6];
// float varianssit[6];
// float summat2[6];
// float varianssit2[6];
// float summat3[6];
// float varianssit3[6];
// enum state { idle=0, lampotila=1, viesti=2, mittaus=3, lampotila_mittaus=4 };
// enum state mystate = idle;
// int button0 = 0;
// int data_tallennus = 1; //viimeisn tallennetut arvot kohta (huom, "1" -> "0")

// /* JTKJ: Display */
// Display_Handle hDisplay;

// //MPU GLOBAL VARIABLES
// static PIN_Handle hMpuPin;
// static PIN_State MpuPinState;
// static PIN_Config MpuPinConfig[] = {
//     Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
//     PIN_TERMINATE
// };

// // MPU9250 uses its own I2C interface
// static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
//     .pinSDA = Board_I2C0_SDA1,
//     .pinSCL = Board_I2C0_SCL1
// };

// /* JTKJ: Pin Button1 configured as power button */
// static PIN_Handle hPowerButton;
// static PIN_State sPowerButton;
// PIN_Config cPowerButton[] = {
//     Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
//     PIN_TERMINATE
// };
// PIN_Config cPowerWake[] = {
//     Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
//     PIN_TERMINATE
// };

// /* JTKJ: Pin Button0 configured as input */
// static PIN_Handle hButton0;
// static PIN_State sButton0;
// PIN_Config cButton0[] = {
//     Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE, // JTKJ: CONFIGURE BUTTON 0 AS INPUT (SEE LECTURE MATERIAL)
//     PIN_TERMINATE
// };

// /* JTKJ: Leds */
// static PIN_Handle hLed;
// static PIN_State sLed;
// PIN_Config cLed[] = {
//     Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, // JTKJ: CONFIGURE LEDS AS OUTPUT (SEE LECTURE MATERIAL)
//     PIN_TERMINATE
// };

// //OMAT FUNKTIOT
// void sensori_main(lista);

// float keskiarvo(float data[][6], int indeksi);

// void datankerays();

// void paattely();

// void update_screen();

// void clkFxn(UArg arg0);

// /* JTKJ: Handle for power button */
// void powerButtonFxn(PIN_Handle handle, PIN_Id pinId) {
//     // Vaihdetaan led-pinnin tilaa negaatiolla
//     PIN_setOutputValue( hLed, Board_LED0, !PIN_getOutputValue( Board_LED0 ) );
    
//   //Tilojen vaihto alemmasta napista
//   	if (mystate == 0) {
//   	    mystate = 1;
//   	} else if (mystate == 1) {
//   	    mystate = 2;
//   	} else if (mystate == 2) {
//         mystate = 0;
//   	} 
// }

// /* JTKJ: WRITE HERE YOUR HANDLER FOR BUTTON0 PRESS */
// void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
//     //Virran sammutus jos idle tai mittaus    
//     if (mystate == 0) {
//         mystate = 3;
//     }
//     /*if ((mystate == 0) || (mystate == 3)) {
//         Display_clear(hDisplay);
//         Display_close(hDisplay);
//         Task_sleep(100000 / Clock_tickPeriod);

// 	    PIN_close(hPowerButton);

//         PINCC26XX_setWakeup(cPowerWake);
// 	    Power_shutdown(NULL,0);
//   	}*/
//   	else if (mystate == 1) {
//         mystate = 4;
//         // Vaihdetaan led-pinnin tilaa negaatiolla
//         PIN_setOutputValue( hLed, Board_LED0, !PIN_getOutputValue( Board_LED0 ) );
//     } 
//     else if (mystate == 2) {
//         button0 = 1; //viestin lähetys
//         System_printf("Viesti lahetetty\n");
//         System_flush();
//     }
//     else if (mystate == 3) {
//         mystate = 0;
//     }
// }

// /* JTKJ: Communication Task */
// Void commTask(UArg arg0, UArg arg1) {
//     char payload[16];
//     char payload1[16];
//     uint16_t senderAddr;
    
//     // Radio to receive mode
//     int32_t result = StartReceive6LoWPAN();
//     if(result != true) {
//       System_abort("Wireless receive start failed");
//     }

//     // Aina lähetyksen jälkeen pitää palata vastaanottotilaan
//     StartReceive6LoWPAN();
    
//     while (1) {

//         if (button0 == 1) {
//             sprintf(payload,"numero %x", IEEE80154_MY_ADDR);
//             Send6LoWPAN(IEEE80154_SERVER_ADDR, payload, strlen(payload));
//             button0 = 0;
//         } 

//         // jos true, viesti odottaa
//         if (GetRXFlag()) {

//             // Tyhjennetään puskuri (ettei sinne jäänyt edellisen viestin jämiä)
//             memset(payload1,0,16);
//             // Luetaan viesti puskuriin payload
//             Receive6LoWPAN(&senderAddr, payload1, 16);
//             // Tulostetaan vastaanotettu viesti konsoli-ikkunaan
//             System_printf(payload1);
//             System_printf("\n");
//             System_flush();
//         }
// }
//         // DO __NOT__ PUT YOUR SEND MESSAGE FUNCTION CALL HERE!! 

//     	// NOTE THAT COMMUNICATION WHILE LOOP DOES NOT NEED Task_sleep
//     	// It has lower priority than main loop (set by course staff)
// }        

// /* JTKJ: laboratory exercise task */
// Void labTask(UArg arg0, UArg arg1) {

//     I2C_Handle i2c; // INTERFACE FOR OTHER SENSORS
//     I2C_Params i2cParams;
//     I2C_Handle i2cMPU; // INTERFACE FOR MPU9250 SENSOR
// 	I2C_Params i2cMPUParams;
	
// 	double pressure;
// 	double temperature;
// 	char tulostettava[50];

//     //jtkj: Create I2C for usage 
//     I2C_Params_init(&i2cParams);
//     i2cParams.bitRate = I2C_400kHz;
    
//     I2C_Params_init(&i2cMPUParams);
//     i2cMPUParams.bitRate = I2C_400kHz;
//     i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

//     i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
//     if (i2cMPU == NULL) {
//         System_abort("Error Initializing I2CMPU\n");
//     }
//     //MPU PWER ON
//     PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

//     // WAIT 100MS FOR THE SENSOR TO POWER UP
// 	Task_sleep(100000 / Clock_tickPeriod);
//     System_printf("MPU9250: Power ON\n");
//     System_flush();
    
    
//     // MPU9250 SETUP AND CALIBRATION
//         System_printf("MPU9250: Setup and calibration...\n");
//     	System_flush();
    
//     	mpu9250_setup(&i2cMPU);
    
//     	System_printf("MPU9250: Setup and calibration OK\n");
//     	System_flush();
	
// 	I2C_close(i2cMPU); //CLOSE MPU
	
	
// 	i2c = I2C_open(Board_I2C0, &i2cParams);
//     if (i2c == NULL) {
//         System_abort("Error Initializing I2C\n");
//     }
    
//     // JTKJ: SETUP BMP280 SENSOR HERE
//     bmp280_setup(&i2c);

//     Task_sleep(1 * 1000000/Clock_tickPeriod); 
    
//     I2C_close(i2c);
    
//     // JTKJ: Init Display 
//     Display_Params displayParams;
// 	displayParams.lineClearMode = DISPLAY_CLEAR_BOTH;
//     Display_Params_init(&displayParams);

//     hDisplay = Display_open(Display_Type_LCD, &displayParams);
//     if (!hDisplay) {
//         System_abort("Naytto ei toimi");
//     }
//     Display_clear(hDisplay);

//     char str[50];
//     sprintf(str,"%4x", IEEE80154_MY_ADDR);
//     Display_print0(hDisplay, 5, 5, str); 
    
//     //Tyhjennä näyttö
//     Display_clear(hDisplay);

    
//         i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
// 	    if (i2cMPU == NULL) {
// 	        System_abort("Error Initializing I2CMPU\n");
// 	    }

//     // JTKJ: main loop
//     while (1) {

//         switch (mystate) {
//             case 0:
//                 // MENU 0
//                 update_screen();
//                 break;

//             case 1:
//                 //MENU 1
//                 update_screen();
//                 break;
                
//             case 2:
//                 //MENU 2
//                 update_screen();
//                 break;
                
//             case 3:
//                 //MITTAUSTILA
//                 update_screen();
//                 sensori_main(datalista); 
//                 mystate = 0;
//                 break;
            
//             case 4:
//                 //LAMPÖTILA MITTAUS
//                 Display_clear(hDisplay);
//                 I2C_close(i2cMPU); // Close MPU for BMP
    	
//             	i2c = I2C_open(Board_I2C, &i2cParams);
//         	    if (i2c == NULL) {
//         	        System_abort("Error Initializing I2C\n");
//         	    }
            	
//             	bmp280_get_data(&i2c, &pressure, &temperature);
//                 sprintf(tulostettava,"Lämpötila on %.1f C ja paine on %.0f Pascalia. \n", temperature, pressure);
//             	System_printf(tulostettava);
//                 System_flush();
                
//                 char str2[50];
//                 sprintf(str2,"%.1f C", temperature);
//                 Display_print0(hDisplay, 1, 1, "Lampotila:");
//                 Display_print0(hDisplay, 3, 1, str2);
                
//                 char str3[50];
//                 sprintf(str3,"%.0f Pa", pressure);
//                 Display_print0(hDisplay, 6, 1, "Ilmanpaine:");
//                 Display_print0(hDisplay, 8, 1, str3);
                
//                 I2C_close(i2c); //Close BMP for MPU
                
//                 i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
// 	            if (i2cMPU == NULL) {
// 	                System_abort("Error Initializing I2CMPU\n");
// 	            }
// 	            Task_sleep(3000000 / Clock_tickPeriod); //Näytetään lämpötila 3s
// 	            Display_clear(hDisplay);
// 	            mystate = 1;
//                 break;
            
//         }
//     	System_flush();
        
//         // I2C_close(i2cMPU);
        
//     	// JTKJ: Do not remove sleep-call from here!
//     	Task_sleep(400000 / Clock_tickPeriod);
//     	// Display_clear(hDisplay);
//     }
//     PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
// }


// // Taskifunktio
// Int main(void) {

//     // Task variables
// 	Task_Handle hLabTask;
// 	Task_Params labTaskParams;
// 	Task_Handle hCommTask;
// 	Task_Params commTaskParams;
// 	Clock_Handle clkHandle;
//     Clock_Params clkParams;

//     // Initialize board
//     Board_initGeneral();
//     Board_initI2C();

// 	// Initialize clock
// 	Clock_Params_init(&clkParams);
//     clkParams.period = 7000000 / Clock_tickPeriod; 
//     clkParams.startFlag = TRUE;
    
//     clkHandle = Clock_create((Clock_FuncPtr)clkFxn, 1000000 / Clock_tickPeriod, &clkParams, NULL);
//     if (clkHandle == NULL) {
//       System_abort("Clock creat failed");
//     }
	
// 	/* JTKJ: Power Button */
// 	hPowerButton = PIN_open(&sPowerButton, cPowerButton);
// 	if(!hPowerButton) {
// 		System_abort("Error initializing power button shut pins\n");
// 	}
// 	if (PIN_registerIntCb(hPowerButton, &powerButtonFxn) != 0) {
// 		System_abort("Error registering power button callback function");
// 	}

//     // JTKJ: INITIALIZE BUTTON0 HERE
//     hButton0 = PIN_open(&sButton0, cButton0);
// 	if(!hButton0) {
// 		System_abort("Error initializing power button shut pins\n");
// 	}
// 	if (PIN_registerIntCb(hButton0, &buttonFxn) != 0) {
// 		System_abort("Error registering power button callback function");
// 	}

//     /* JTKJ: Init Leds */
//     hLed = PIN_open(&sLed, cLed);
//     if(!hLed) {
//         System_abort("Error initializing LED pin\n");
//     }
    
//     /* MPU POWER PIN */
//     hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
//     if (hMpuPin == NULL) {
//     	System_abort("Pin open failed!");
//     }

//     /* JTKJ: Init Main Task */
//     Task_Params_init(&labTaskParams);
//     labTaskParams.stackSize = STACKSIZE;
//     labTaskParams.stack = &labTaskStack;
//     labTaskParams.priority=2;

//     hLabTask = Task_create(labTask, &labTaskParams, NULL);
//     if (hLabTask == NULL) {
//     	System_abort("Task create failed!");
//     }

//     /* JTKJ: Init Communication Task */
//     Init6LoWPAN();

//     Task_Params_init(&commTaskParams);
//     commTaskParams.stackSize = STACKSIZE;
//     commTaskParams.stack = &commTaskStack;
//     commTaskParams.priority=1;
    
//     hCommTask = Task_create(commTask, &commTaskParams, NULL);
//     if (hCommTask == NULL) {
//     	System_abort("Task create failed!");
//     }

//     // JTKJ: Send hello to console
//     System_printf("Hello world!\n");
//     System_flush();

//     /* Start BIOS */
//     BIOS_start();

//     return (0);
// }

// //OMAT FUNKTIOT

// void sensori_main(lista) {
//     int j;
//     char tulostus[150];
//     char tulostus2[150];
//     char tulostus3[10];
    
//     datankerays();
//     System_printf("Datan kerays ok\n");
// 	System_flush();
// 	/*for (j=0; j<10; j++) {
//         sprintf(tulostus3,"%f\n", datalista[j][2]);
//         System_printf(tulostus3);
// 	}
// 	System_flush();*/
    
//     for (j=0; j<6; j++) {
//         keskiarvo(lista, j); 
//     }
//     sprintf(tulostus,"Variansseja: [%.4f, %.4f, %.4f] [%.4f, %.4f, %.4f] [%.4f, %.4f, %.4f] \n", varianssit[0], varianssit[1], varianssit[2], varianssit2[0], varianssit2[1], varianssit2[2], varianssit3[0], varianssit3[1], varianssit3[2]);
//     System_printf(tulostus);
//     sprintf(tulostus2,"X, Y ja Z keskiarvot: %.2f, %.2f %.2f \n", summat[0], summat[1], summat[2]);
//     System_printf(tulostus2);
//     System_flush();
    
//     paattely();
// }

// float keskiarvo(float data[][6], int indeksi) {
//     int i;
//     int j;
//     int n = 0;
//     char tulostus[200];
//     float summa = 0;
//     float var_summa = 0;
//     float varianssi = 0;
//     int kerroin;
    
//     if (data_tallennus == 1) {
// 	    kerroin = 0;
// 	} else if (data_tallennus == 2) {
// 	    kerroin = 20;
// 	} else if (data_tallennus == 3) {
// 	    kerroin = 40;
// 	}
//     for (i=0; i<20; i++) { //muuta siten että pystyy laskemaan listasta kun on arvoja jotka on tyhjiä
//         summa += data[kerroin + i][indeksi];
//         n += 1;
//     }
//     summa = summa / n;
    
//     for (j=0; j<n+1; j++) {
//         var_summa += pow((data[kerroin + j][indeksi] - summa), 2);
//     }
//     varianssi = var_summa / (n - 1);    
    
//     if (data_tallennus == 1) {
//         summat[indeksi] = summa; //SIJOITUKSET GLOBAALEIHIN
//         varianssit[indeksi] = varianssi;
//     } 
//     else if (data_tallennus == 2) {
//         summat2[indeksi] = summa; 
//         varianssit2[indeksi] = varianssi;
//     }
//      else if (data_tallennus == 3) {
//         summat3[indeksi] = summa; 
//         varianssit3[indeksi] = varianssi;
//     }
    
//     /*sprintf(tulostus,"Keskiarvo on: %.4f \nVarianssi on: %.4f \n\n", summa, varianssi);
//     System_printf(tulostus);
// 	System_flush();*/
    
//     return 0;
// }

// void datankerays() {
//     I2C_Handle i2cMPU; // INTERFACE FOR MPU9250 SENSOR
// 	I2C_Params i2cMPUParams;
// 	float ax, ay, az, gx, gy, gz;
//     int i;
//     int x;
//     char tulostus3[10];
//     int kerroin;
// 	if (data_tallennus == 1) {
// 	    kerroin = 0;
// 	} else if (data_tallennus == 2) {
// 	    kerroin = 10;
// 	} else if (data_tallennus == 3) {
// 	    kerroin = 20;
// 	}
// 	for (x=0; x<20; x++) {
// 	//while (mystate == 3) {
//         mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
//         //int i = 1;
//         i = kerroin + x; 
//         datalista[i][0] = ax;
//         datalista[i][1] = ay;
//         datalista[i][2] = az;
//         datalista[i][3] = gx;
//         datalista[i][4] = gy;
//         datalista[i][5] = gz;
//         sprintf(tulostus3,"%f\n", datalista[i][2]);
//         System_printf(tulostus3);
//         System_flush();
	    
//         Task_sleep(1 * 100000/Clock_tickPeriod); //Datan keräys 0.1s välein
//     }
//     if (data_tallennus > 3) {
//         data_tallennus = 1;
//     } else {
//         data_tallennus += 1;
//     }
// }


// void paattely() {
//     if (varianssit[2] > 0.02) {
//         //Portaissa tai kävely
//         if (summat[2] > -0.5) {
//             System_printf("Portaissa ylospain\n");
// 	        System_flush();
//         }else if (summat[2] < -0.5) {
//             System_printf("Portaissa alaspain\n");
// 	        System_flush();
//         }
//     }
//     else if (varianssit[2] < 0.02) {
//         //Hississa 
//         if (summat[2] < -1.0) {
//             System_printf("Hississa ylospain\n");
// 	        System_flush();
//         }else if (summat[2] < -1.5) {
//             System_printf("Hississa alaspain\n");
// 	        System_flush();
//         }
//     }
// }

// // Taskifunktio
// Void displayFxn(UArg arg0, UArg arg1) {

//   Display_Params params;
//   Display_Params_init(&params);
//   params.lineClearMode = DISPLAY_CLEAR_BOTH;

//   Display_Handle hDisplayLcd = Display_open(Display_Type_LCD, &params);

//   if (hDisplayLcd) {
      
//       // Grafiikkaa varten tarvitsemme lisää RTOS:n muuttujia
//       tContext *pContext = DisplayExt_getGrlibContext(hDisplayLcd);

//       if (pContext) {

//          // Piirretään puskuriin kaksi linjaa näytön poikki x:n muotoon
//          GrLineDraw(pContext,0,0,96,96);
//          GrLineDraw(pContext,0,96,96,0);

//          // Piirto puskurista näytölle
//          GrFlush(pContext);
//       }
//   }
// }

// void update_screen() {
//     if (mystate == 0) {
//         Display_print0(hDisplay, 1, 1, "Idle");
//         Display_print0(hDisplay, 3, 1, "o MITTAUSTILA");
//         Display_print0(hDisplay, 5, 3, "LAMPOTILA");
//         Display_print0(hDisplay, 7, 3, "VIESTIT");
//     } else if (mystate == 1) {
//         Display_print0(hDisplay, 1, 1, "Idle");
//         Display_print0(hDisplay, 3, 3, "MITTAUSTILA");
//         Display_print0(hDisplay, 5, 1, "o LAMPOTILA");
//         Display_print0(hDisplay, 7, 3, "VIESTIT");
//     } else if (mystate == 2) {
//         Display_print0(hDisplay, 1, 1, "Idle");
//         Display_print0(hDisplay, 3, 3, "MITTAUSTILA");
//         Display_print0(hDisplay, 5, 3, "LAMPOTILA");
//         Display_print0(hDisplay, 7, 1, "o VIESTIT");
//     } else if (mystate == 3) {
//         Display_print0(hDisplay, 1, 1, "Mittaa...");
//         Display_print0(hDisplay, 3, 3, "MITTAUSTILA");
//         Display_print0(hDisplay, 5, 3, "LAMPOTILA");
//         Display_print0(hDisplay, 7, 3, "VIESTIT");
//     }
// }

// void clkFxn(UArg arg0) {
//     // Muutetaan tilaa halutuksi
//     // Huom! If-lauseella asetetaan, että tilasiirto on mahdollinen 
//     // vain jos nykyinen tila on IDLE!

//     /*if (mystate == 0) {
//         mystate = 3;
//     }*/
// }

    