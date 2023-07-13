/*******************************************************************************
* File Name: main.c
*
* Version: 2.0
*
* Description:
*   The component is enumerated as a Virtual Com port. Receives data from the 
*   hyper terminal, then sends back the received data.
*   For PSoC3/PSoC5LP, the LCD shows the line settings.
*
* Related Document:
*  Universal Serial Bus Specification Revision 2.0
*  Universal Serial Bus Class Definitions for Communications Devices
*  Revision 1.2
*
********************************************************************************
* Copyright 2015, Cypress Semiconductor Corporation. All rights reserved.
* This software is owned by Cypress Semiconductor Corporation and is protected
* by and subject to worldwide patent and copyright laws and treaties.
* Therefore, you may use this software only as provided in the license agreement
* accompanying the software package from which you obtained this software.
* CYPRESS AND ITS SUPPLIERS MAKE NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* WITH REGARD TO THIS SOFTWARE, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT,
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*******************************************************************************/

#include <project.h>
#include "stdio.h"
#include "stdlib.h"


#if defined (__GNUC__)
    /* Add an explicit reference to the floating point printf library */
    /* to allow usage of the floating point conversion specifiers. */
    /* This is not linked in by default with the newlib-nano library. */
    asm (".global _printf_float");
#endif

#define USBFS_DEVICE    (0u)

/* The buffer size is equal to the maximum packet size of the IN and OUT bulk
* endpoints.
*/

//Stuff for USB communication
#define CHANGE_FPS 0x1
#define CHANGE_Z_STEPS 0x2
#define SET_READOUT_SPEED 0x4
#define SLOW_READOUT 0x8
#define SET_LASER_MODE 0x10
#define COUNTING 0x20
#define STOP_COUNT 0x40
#define SET_RUN_MODE 0x100
#define SET_SIM_MODE 0x200
#define START_CAPTURE 0x800
#define STOP_CAPTURE 0x1000
#define STAGE_TRIGG_ENABLE 0x40000
#define STAGE_TRIGG_DISABLE 0x80000
#define SEND_TRIGG 0x100000
#define SET_EXPOSURE 0x200000
#define START_LIVE 0x100000
#define LIVE_RUNNING 0x200000
#define STOP_LIVE 0x400000
#define STAGE_MOVE_COMPLETE 0x800000
#define Z_STACK_RUNNING 0x1000000
#define STOP_Z_STACK 0x2000000
#define TOGGLE_BLANKING 0x4000000
#define TOGGLE_DIG_MOD 0x8000000

/* USB device number. */
#define USBFS_DEVICE  (0u)

/* Active endpoints of USB device. */
#define IN_EP_NUM     (1u)
#define OUT_EP_NUM    (2u)

/* Size of SRAM buffer to store endpoint data. */
#define BUFFER_SIZE   (64u)

struct usb_data{
    float fps;
    float exposure;
    uint32 flags;
    uint16 steps;
    uint8 mode;
    uint8 bonus;
    uint64 count;
};
volatile struct usb_data incoming;
volatile struct usb_data outgoing;

#define DEFAULT_FPS (10.0f)

#if (USBFS_16BITS_EP_ACCESS_ENABLE)
    /* To use the 16-bit APIs, the buffer has to be:
    *  1. The buffer size must be multiple of 2 (when endpoint size is odd).
    *     For example: the endpoint size is 63, the buffer size must be 64.
    *  2. The buffer has to be aligned to 2 bytes boundary to not cause exception
    *     while 16-bit access.
    */
    #ifdef CY_ALIGN
        /* Compiler supports alignment attribute: __ARMCC_VERSION and __GNUC__ */
        CY_ALIGN(2) uint8 buffer[BUFFER_SIZE];
    #else
        /* Complier uses pragma for alignment: __ICCARM__ */
        #pragma data_alignment = 2
        uint8 buffer[BUFFER_SIZE];
    #endif /* (CY_ALIGN) */
#else
    /* There are no specific requirements to the buffer size and alignment for 
    * the 8-bit APIs usage.
    */
    uint8 buffer[BUFFER_SIZE];
#endif /* (USBFS_GEN_16BITS_EP_ACCESS) */

#define USBUART_BUFFER_SIZE (64u)
#define LINE_STR_LENGTH     (20u)

#define COUNT_PERIOD (0.0000005f) // 2MHz counter ticks 500ns
#define SLM_CNTR_TICKS (5360u)    // 1.18ms + 1.5ms
#define SLM_TRG_TICKS (200u)      // 50us
//#define ANDOR_READ_TIME (2000u)   // 1MHz worst case is 1ms
#define THIRTY_FPS (66667u)       // 33.3ms
#define THIRTY_EXP (40955u)       // 33.3ms - 1ms - 1.18ms * 2
#define THIRTY (30.0f)
#define TEN (10.0f)
#define TEN_FPS (200000u)
#define TEN_EXP (77200u)        // 100ms - 1ms - 1.18ms * 2 - 56.8ms work damn you!
#define TWENTY_FOUR (24.0f)
#define TWENTY (20.0f)
#define TWENTY_FPS (100000u)
#define TWENTY_FOUR_FPS (83333u)
#define TWENTY_EXP (3940u)       // 50ms - 1ms -1.18ms*2 - 39ms -4.43ms = 1.97ms
#define FOURTY_EIGHT (48.0f)
#define FOURTY_EIGHT_FPS (41666u)
#define TWENTY_SIX_FPS (76923u)
#define TWENTY_SIX (26.0f)
#define STAGE_TICKS (180000u)     // Xms for stage trigger
//#define STAGE_TICKS (12000u)
#define ANDOR_READOUT (78600u)
#define ANDOR_DELAY_TICKS (2048u)
#define HAMA_SLOW_TICKS (650u)
#define HAMA_NORM_TICKS (195u)
#define HAMA_SLOW_READ (59586u)//(80920)//(58920u)  // 33ms - 1.18ms * 3
//#define ANDOR_30_MHZ_HORZ (.00003837f)
#define ANDOR_30_MHZ_HORZ (.00005547f) // Andors Timing chart is bulshit
#define HAMA_SLOW_HORZ (.0000324812f) 
#define HAMA_NORM_HORZ (.00000974436f)
#define HAMA_VERT_DEFAULT (2048u)
#define ANDOR_VERT_DEFAULT (1024u)
#define CAP_MODE_NORMAL (0u)
#define CAP_MODE_SLOW (1u)

#define THREE_BEAM (0u)
#define TWO_BEAM (1u)
#define NO_SIM_Z_ONLY (2u)
#define SINGLE_ANGLE (3u)
#define SINGLE_ANGLE_MAX (5u)
#define THREE_BEAM_MAX (15u)
#define TWO_BEAM_MAX (9u)
#define NO_BEAM_MAX (1u)
#define TWO_CAM_MIN (2u)

#define BLANK_ON (0u)
#define BLANK_OFF (1u)

#define ENTR_FRAME_RATE "\n\nEnter FPS float: "
#define ENTR_Z_STEPS "\n\nEnter Z-Steps: "
#define ENTER_TIME_SECS "\n\nEnter Time(secs) float: "
#define ENTER_VERT_VALUE "\n\nEnter Vertical Pixels: "
#define ENTER_CAP_MODE "\n\nSet Capture Mode:\n 0) Normal\n 1) Slow\nEnter Choice: "
#define SELECT_MODE "\n\nSet Mode:\n 0) Free Run\n 1) Z-mode\n 2) Timed\nEnter Choice: "
#define SIM_SELECT "\n\nSet SIM Mode:\n 0) Three beam\n 1) Two beam\n "
#define SIM_SELECT2 "2) Z only\n 3) Single Angle\nEnter Choice: "
//#define SET_EXPOSURE "\n\nEnter Exposure float: "
#define LASER_SELECT "\n\n Laser(s) to use:\n 0) 588nm\n 1) 561nm\n 2) Both alternating"
#define LASER_SELECT2 "\nEnter Choice: "
#define OPTIONS "\n\n\n\n a) set FPS\n b) set z-steps\n c) time to capture"
#define OPTIONS2 "\n d) vert dim\n e) capture mode\n f) run mode"
#define OPTIONS3 "\n g) start\n h) stop\n i) blanking\n j) SIM Beams"
#define OPTIONS4 "\n k) Show Config\n l) Set exposure time"
#define OPTIONS5 "\n m) LASERS\nEnter Choice:"
#define TRIGG_ON "Trig: ON "
#define TRIGG_OFF "Trig: OFF"
#define CAMERA_TRIGGER (0u)
//#define SLM_TRIGGER (1u)
#define STAGE_TRIGGER (1u)
#define REG_ON (1u)
#define REG_OFF (0u)
#define FREE_RUN (0u)
#define Z_MODE (1u)
#define TIMED_MODE (2u)
#define BLUE_LASER (0u)
#define GREEN_LASER (1u)
#define BOTH_LASERS (2u)

#define TIMED_FIN 0x01
#define Z_FIN     0x02
#define STUPID (0u)//(200000u)

//char8* parity[] = {"None", "Odd", "Even", "Mark", "Space"};
//char8* stop[]   = {"1", "1.5", "2"};


/*******************************************************************************
* Function Name: main
********************************************************************************
*
* Summary:
*  The main function performs the following actions:
*   1. Waits until VBUS becomes valid and starts the USBFS component which is
*      enumerated as virtual Com port.
*   2. Waits until the device is enumerated by the host.
*   3. Waits for data coming from the hyper terminal and sends it back.
*   4. PSoC3/PSoC5LP: the LCD shows the line settings.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
/******** The Land Of Globals ********/
uint8 deMux = CAMERA_TRIGGER;
volatile uint8 phases = 0;
volatile uint16 zCount = 0;
uint16 vert = ANDOR_VERT_DEFAULT;
uint8 capMode = CAP_MODE_NORMAL;
double horzPeriod = ANDOR_30_MHZ_HORZ;
double readOutTime = 0;
volatile uint8 mode = FREE_RUN;
volatile uint8 simMode = THREE_BEAM;
volatile uint16 zSteps = 0;
volatile uint32 frameTicks = TEN_FPS;
volatile uint32 exposureTicks = TEN_EXP;
volatile float exposureSeconds = 0.0386f;
volatile double exposure = 0.0386f;
volatile double exposureMax = 0.00386f;
volatile uint32 time_ticks_rem = 0;
uint32 time_ticks = 0;
uint32 wait_time_ticks = 0;
volatile uint8 msgFlag = 0;
volatile uint8 phase_max = THREE_BEAM_MAX;
volatile uint8 laser_conf = BLUE_LASER;
volatile uint8 to_send = 0;

char line0[20];
char line1[20];
char line2[20];
char line3[20];

double fps_in = TEN;


char msg[64];

CY_ISR(T_ISR){
    
    // Controls the trigger periods and
    // which device gets the trigger pulse
    //STAGE_WAIT_REG_Write(REG_OFF);
    ENBL_TRIG_ISR_Write(REG_OFF);
    TRIG_ISR_ClearPending();
    //outgoing.flags &= ~SEND_TRIGG;
    
    if (phases < phase_max) {
            if(laser_conf == BOTH_LASERS){
                /* First Laser Should be set to Green @ start*/
                /* Just to make sure switches to Blue */
                uint8 val = CAM_SEL_REG_Read();
                CAM_SEL_REG_Write(!val);
                if (val == BLUE_LASER){
                    phases++;
                    
                }
            } else {
                phases++;
            }
            //TRG_CNT_WritePeriod(exposureTicks);

            TRIG_CNT_RST_Write(REG_ON);
            TRIG_CNT_RST_Write(REG_OFF);
            //TRIG_ISR_ClearPending();
            ENBL_TRIG_ISR_Write(REG_ON);
            //STAGE_WAIT_REG_Write(REG_ON);

            if(mode == TIMED_MODE){
                if(time_ticks_rem > frameTicks){
                    time_ticks_rem -= frameTicks;
                } else {
                    time_ticks_rem = 0;
                    ENBL_TRIG_ISR_Write(REG_OFF);
                    msgFlag |= TIMED_FIN;
                }
            }
        //}
    } else {
    //if (phases > 14){
        phases = 0;
        //strcpy(msg, "\nphases = 0\n");
        /* Wait until component is ready to send data to host. */
        //while (0u == USBUART_CDCIsReady())
        //{
        //}
        /* Send MSG back to host. */
        //USBUART_PutData((uint8*)msg, strlen(msg));
        
        if(mode == Z_MODE){
            
            if (zCount < zSteps) {
                

                //outgoing.flags |= SEND_TRIGG;
                STAGE_REG_Write(REG_ON);
                STAGE_REG_Write(REG_OFF);
                zCount++;
                ENBL_TRIG_ISR_Write(REG_ON);
                //TRIG_ISR_ClearPending();// Trying to see if this reduces stage movements.
            } else {
                ENBL_TRIG_ISR_Write(REG_OFF); 
                //STAGE_WAIT_REG_Write(REG_ON);
                zCount = 0;
                msgFlag |= Z_FIN;
            }
        } else {
            ENBL_TRIG_ISR_Write(REG_OFF);
            ENBL_TRIG_ISR_Write(REG_ON);
        }
       
    }

}
// read input prototype
void read_input(volatile struct usb_data* buf, const char cmd);
void set_mode(uint8 buf);
void set_sim_mode(uint8 buf);
void set_laser_mode(uint8 laser_mode);
void set_capMode(uint32 buf);
void setExposure(void);
void userSetExposure(void);
void setWaitTime(void);
void readTime(void);

int main()
{
    
    incoming.flags = outgoing.flags = 0;
    incoming.fps = outgoing.fps = DEFAULT_FPS;
    outgoing.count = 0;
    outgoing.exposure = exposure;
    //uint16 count;
    
#if (CY_PSOC3 || CY_PSOC5LP)
    
    LCD_Char_Start();
#endif /* (CY_PSOC3 || CY_PSOC5LP) */
    
    CyGlobalIntEnable;

    //PWM_Start();  No more PWM
    TRG_CNT_Start();
    SLM_WAIT_Start();
    SLM_TRIG_Start();
    BLANKING_DELAY_Start();
    STAGE_TRIG_Start();
    STAGE_WAIT_Start();
    TRIG_ISR_StartEx(T_ISR);

    /* Start USBFS operation with 5V operation. */
    USBFS_Start(USBFS_DEVICE, USBFS_5V_OPERATION);
        /* Wait until device is enumerated by host. */
    while (0u == USBFS_GetConfiguration())
    {
    }
    
    /* Enable OUT endpoint to receive data from host. */
    USBFS_EnableOutEP(OUT_EP_NUM);
    //CyDelay(1000);
    /* Service USB CDC when device is configured. */
    //if (0u != USBUART_GetConfiguration())
    //{
    //    strcpy(msg, OPTIONS);
        /* Send  OPTIONS. */
    //    USBUART_PutData((uint8*)msg, strlen(msg));
    //}
    readTime();
    setWaitTime();
    SLM_TRIG_WritePeriod(SLM_TRG_TICKS);
    //SLM_WAIT_WritePeriod(wait_time_ticks);
    TRIG_CNT_RST_Write(REG_ON);
    ACTIVATE_SLM_Write(REG_ON);
    STAGE_REG_Write(REG_ON);
    CyDelay(100);
    TRIG_CNT_RST_Write(REG_OFF);
    ACTIVATE_SLM_Write(REG_OFF);
    STAGE_REG_Write(REG_OFF);
    BLANKING_DELAY_WritePeriod(ANDOR_DELAY_TICKS);
    STAGE_WAIT_WritePeriod(10000);
    STAGE_TRIG_WritePeriod(5000);
    BLANK_TOGGLE_Write(BLANK_ON);
    
    LCD_Char_Position(0u, 0u);
    LCD_Char_PrintString("Mode: Free");
    LCD_Char_Position(0u, 11u);
    LCD_Char_PrintString("FPS: 10.0");
    LCD_Char_Position(1u, 0u);
    LCD_Char_PrintString(TRIGG_OFF);
    LCD_Char_Position(1u, 10u);
    LCD_Char_PrintString("Blank: Off");
    setExposure();
    for(;;)
    {
        /* Check if configuration is changed. */
        if (0u != USBFS_IsConfigurationChanged())
        {
            /* Re-enable endpoint when device is configured. */
            if (0u != USBFS_GetConfiguration())
            {
                /* Enable OUT endpoint to receive data from host. */
                USBFS_EnableOutEP(OUT_EP_NUM);
            }
        }
        
                /* Check if data was received. */
        if (USBFS_OUT_BUFFER_FULL == USBFS_GetEPState(OUT_EP_NUM))
        {
            /* Read number of received data bytes. */
            //length = USBFS_GetEPCount(OUT_EP_NUM);

            /* Trigger DMA to copy data from OUT endpoint buffer. */

            USBFS_ReadOutEP(OUT_EP_NUM, (uint8*)&incoming, sizeof(struct usb_data));
            /*if(incoming.flags & SEND_TRIGG){
                    SOFT_TRIG_Reg_Write(REG_ON);
                    CyDelayUs(100);
                    SOFT_TRIG_Reg_Write(REG_OFF);
                    incoming.flags &= ~(SEND_TRIGG);
                    outgoing.flags |= TRIGG_SENT;
            }*/

                if(incoming.flags & STAGE_MOVE_COMPLETE){
                    //STAGE_WAIT_REG_Write(REG_ON);
                    incoming.flags &= ~(STAGE_MOVE_COMPLETE);
                }
                if(incoming.flags & CHANGE_FPS){
                    read_input(&incoming, 'a');
                    SLM_WAIT_WritePeriod(wait_time_ticks);
                    outgoing.fps = incoming.fps;
                    incoming.flags &= ~(CHANGE_FPS);
                }
                if(incoming.flags & CHANGE_Z_STEPS){
                    read_input(&incoming, 'b');
                    outgoing.steps = incoming.steps;
                    incoming.flags &= ~(CHANGE_Z_STEPS);
                }
                    //read_input(buffer, 'c');
                    /* Enter Vertical size of crop */
                    //read_input(buffer, 'd');
                    //readTime();
                    //SLM_WAIT_WritePeriod(wait_time_ticks);
                if(incoming.flags & SET_READOUT_SPEED){
                    /* Change Hammamatsu Camera readout speed */
                    /**** currently just set to Andor readout since andor is REALLLLLY SLOWWWWWWWWWWWW ****/
                    /**** Need to update Settings for slower sensor readout for even less exposure time ***/
                    set_capMode(incoming.flags&SLOW_READOUT);
                    if(laser_conf == BLUE_LASER){
                        if(capMode == CAP_MODE_NORMAL){
                            horzPeriod = ANDOR_30_MHZ_HORZ;
                            BLANKING_DELAY_WritePeriod(ANDOR_DELAY_TICKS);
                        } else {
                            horzPeriod = ANDOR_30_MHZ_HORZ;
                            BLANKING_DELAY_WritePeriod(ANDOR_DELAY_TICKS);
                        }
                    } else if (laser_conf == GREEN_LASER){
                        horzPeriod = ANDOR_30_MHZ_HORZ;
                        BLANKING_DELAY_WritePeriod(ANDOR_DELAY_TICKS); 
                    } else {
                        //horzPeriod = 0;
                        BLANKING_DELAY_WritePeriod(ANDOR_DELAY_TICKS);
                    }
                    setExposure();
                    SLM_WAIT_WritePeriod(wait_time_ticks);
                    incoming.flags &= ~SET_READOUT_SPEED;
                }
                
                if(incoming.flags & SET_RUN_MODE){
                    /* Select between, Free Run, Z-stack and timed mode */
                    set_mode(incoming.mode);
                    incoming.flags &= ~SET_RUN_MODE;
                }
                if(incoming.flags & START_CAPTURE){
                    if(!ENBL_TRIG_ISR_Read()){
                        time_ticks_rem = time_ticks;
                        phases = 0;
                        zCount = 0;
                        if(laser_conf == BOTH_LASERS){
                            CAM_SEL_REG_Write(GREEN_LASER);
                        }
                    }
                    ENBL_TRIG_ISR_Write(REG_ON);
                    LCD_Char_Position(1u,0u);
                    LCD_Char_PrintString(TRIGG_ON);
                    incoming.flags &= ~START_CAPTURE;
                }
                if(incoming.flags & STOP_CAPTURE){
                    ENBL_TRIG_ISR_Write(REG_OFF);
                    LCD_Char_Position(1u,0u);
                    LCD_Char_PrintString(TRIGG_OFF);
                    incoming.flags &= ~STOP_CAPTURE;
                }
                if(incoming.flags & TOGGLE_BLANKING){
                /* Toggle Laser Blanking */
                    if(BLANK_TOGGLE_Read()){
                        BLANK_TOGGLE_Write(BLANK_ON);
                        strcpy(msg, "\n\n****Blanking On****\n\n");
                        LCD_Char_Position(1u,10u);
                        LCD_Char_PrintString("Blank: On ");
                    } else {
                        BLANK_TOGGLE_Write(BLANK_OFF);
                        strcpy(msg, "\n\n****Blanking Off****\n\n");
                        LCD_Char_Position(1u,10u);
                        LCD_Char_PrintString("Blank: Off");
                    }
                    incoming.flags &= ~TOGGLE_BLANKING;
                }
                if(incoming.flags & SET_SIM_MODE){
                    /* For Z stack capture set 2 beam 3 beam or no sim */                        
                    set_sim_mode(incoming.mode);
                    incoming.flags &= ~SET_SIM_MODE;
                }
                    //case 'k': /* Print Out Current Configuration */
                        //if(ENBL_TRIG_ISR_Read()){
                        //    sprintf(msg, "\n\nTrigger: Running\n");   
                        //} else {
                        //    sprintf(msg, "\n\nTrigger: Stopped\n");                             
                        //}
                        
                        /* Wait until component is ready to send data to host. */
                        //while (0u == USBUART_CDCIsReady())
                        //{
                        //}
                        //USBUART_PutData((uint8*)msg, strlen(msg));                        
                        
                        //if(capMode == CAP_MODE_NORMAL){
                        //    sprintf(msg, "Camera Readout Mode: Normal\n");
                        //} else {
                        //     sprintf(msg, "Camera Readout Mode: SLOW\n");   
                        //}
                        /* Send  OPTIONS. */
                        /* Wait until component is ready to send data to host. */
                        //while (0u == USBUART_CDCIsReady())
                        //{
                        //}
                        //USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        //if(mode == 0){
                        //    sprintf(msg, "Mode: Free Run\n");
                        //} else if (mode == 1){
                        //    sprintf(msg, "Mode: Z-Stack\n");
                        //} else {
                        //    sprintf(msg, "Mode: Timed\n");
                        //}
                        
                        /* Wait until component is ready to send data to host. */
                        //while (0u == USBUART_CDCIsReady())
                        //{
                        //}
                        //USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        //if(simMode == 0){
                        //    sprintf(msg, "SIM Mode: Three Beam\n");
                        //} else if(simMode == TWO_BEAM) {
                        //    sprintf(msg, "SIM Mode: Two Beam\n");    
                        //} else if(simMode == NO_SIM_Z_ONLY){
                        //    sprintf(msg, "SIM Mode: Z-only\n");   
                        //} else {
                        //    sprintf(msg, "SIM Mode: Single Angle\n");  
                        //}
                        
                        /* Wait until component is ready to send data to host. */
                        //while (0u == USBUART_CDCIsReady())
                        //{
                        //}
                        //USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        //if(BLANK_TOGGLE_Read()){
                        //    sprintf(msg, "Blanking: Off\n");
                        //} else {
                        //    sprintf(msg, "Blanking: On\n");
                        //}
                        
                        /* Wait until component is ready to send data to host. */
                        //while (0u == USBUART_CDCIsReady())
                        //{
                        //}
                        //USBUART_PutData((uint8*)msg, strlen(msg));                        
                        
                        //sprintf(msg, "FPS: %.3f\n", fps_in);
                        /* Wait until component is ready to send data to host. */
                        //while (0u == USBUART_CDCIsReady())
                        //{
                        //}
                        //USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        //sprintf(msg, "exposure time: %.6f (sec)\n", exposure);                        
                        /* Wait until component is ready to send data to host. */
                        //while (0u == USBUART_CDCIsReady())
                        //{
                        //}
                        //USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        //sprintf(msg, "Z-steps: %u\n", zSteps);
                        
                        /* Wait until component is ready to send data to host. */
                        //while (0u == USBUART_CDCIsReady())
                        //{
                        //}
                        //USBUART_PutData((uint8*)msg, strlen(msg));

                        //if(laser_conf == BLUE_LASER){
                        //    sprintf(msg, "Blue Laser\n");  
                        //} else if (laser_conf == GREEN_LASER){
                        //    sprintf(msg, "Green Laser\n");
                        //} else {
                        //    sprintf(msg, "Both Lasers Alternateing\n");   
                        //}
                        
                        /* Wait until component is ready to send data to host. */
                        //while (0u == USBUART_CDCIsReady())
                        //{
                        //}
                        //USBUART_PutData((uint8*)msg, strlen(msg));
                        //break;
                if(incoming.flags & SET_EXPOSURE){
                    /* Set User specified exposure time */
                    read_input(&incoming,'l');
                    incoming.flags &= ~SET_EXPOSURE;
                    //SLM_WAIT_WritePeriod(wait_time_ticks);
                }
                if(incoming.flags & SET_LASER_MODE){
                    /* Select Laser Mode */
                    set_laser_mode(incoming.mode);
                    incoming.flags &= ~SET_LASER_MODE;
                }
           
        }
        /* Trigger DMA to copy data into IN endpoint buffer.
        * After data has been copied, IN endpoint is ready to be read by the
        * host.
        */
        
          
            //outgoing.time_waiting = time_btwn_trig;
            //outgoing.fps = current_fps;
        uint8 sent = 0;
        
                /* Wait until DMA completes copying data from OUT endpoint buffer. */
        while (USBFS_OUT_BUFFER_FULL == USBFS_GetEPState(OUT_EP_NUM))
        {
        }
        
        /* Enable OUT endpoint to receive data from host. */
        USBFS_EnableOutEP(OUT_EP_NUM);

        /* Wait until IN buffer becomes empty (host has read data). */
        while (USBFS_IN_BUFFER_EMPTY != USBFS_GetEPState(IN_EP_NUM))
        {
        }
        
        if(outgoing.flags & (SEND_TRIGG|STOP_Z_STACK|CHANGE_FPS|SET_EXPOSURE)){
            sent = 1;
        }
            USBFS_LoadInEP(IN_EP_NUM, (uint8*)&outgoing, sizeof(struct usb_data));
            /* (USBFS_GEN_16BITS_EP_ACCESS) */
            //outgoing.flags &= ~(NEW_CNT);
        if(sent){    
            outgoing.flags &= ~(STOP_Z_STACK|SEND_TRIGG|CHANGE_FPS|SET_EXPOSURE);
        }
        //CyDelayUs(250); 
        
        ++outgoing.count;
        switch(msgFlag){
            case Z_FIN:
                msgFlag &= ~Z_FIN;
                outgoing.flags |= STOP_Z_STACK;
                break;
            case TIMED_FIN:
                msgFlag &= ~TIMED_FIN;
                break;
            default:
                break;
        }
    }
}

// helper functions
void read_input(volatile struct usb_data* buf, const char cmd){

    //double time_s;
    switch(cmd){
        case 'a':
            fps_in = buf->fps;
            if(fps_in > 0.0f){
               double period = 1 / fps_in;
               frameTicks = (uint32)(period / COUNT_PERIOD);
            }
            //exposureTicks = frameTicks - SLM_CNTR_TICKS - HAMA_SLOW_READ - SLM_TRG_TICKS;
            setExposure();
            sprintf(msg, "\n\nSetting: %.1f, frameTicks: %lu, exposure: %.6f (sec)\n\n", fps_in, frameTicks, exposure);
            
            sprintf(line0, "FPS: %.1f", fps_in);
            LCD_Char_Position(0u, 11u);
            LCD_Char_PrintString(line0);
            break;
        case 'b':
            zSteps = buf->steps;
            break;
        /*case 'c':
            time_s = strtof(val_str, NULL);
            time_ticks = (uint32)(time_s / COUNT_PERIOD);
            sprintf(msg, "\n\nSetting: %s, time_ticks: %lu\n\n", val_str, time_ticks);
            break;*/
        /*case 'd':
            vert = (uint16)atoi(val_str);
            setExposure();
            sprintf(msg, "\n\nSetting: %s, exposure_ticks: %lu\n\n", val_str, exposureTicks);
            break;*/
        case 'l':
            exposure = buf->exposure;
            userSetExposure();
            //sprintf(msg, "\n\nSetting exposure: %.6f, exposureTicks: %lu\n\n", exposure, exposureTicks);
            break;
        default:
            sprintf(msg, "\n\n****something wrong has happened****\n\n");
            break;
    }
}

void set_mode(uint8 buf){

    mode = buf;   
    sprintf(msg, "\n\nMode: %u\n", mode);

    if(mode == FREE_RUN){
        sprintf(line0, "Mode: Free FPS: %.1f", fps_in);
    } else if(mode == Z_MODE){
        sprintf(line0, "Mode: Z-St FPS: %.1f", fps_in);
    } else {
       sprintf(line0, "Mode: Timed FPS: %.1f", fps_in);   
    }
    
    LCD_Char_Position(0u, 0u);
    LCD_Char_PrintString(line0);
    
}

void set_sim_mode(uint8 buf){

    simMode = buf;   

    if(simMode == TWO_BEAM){
        phase_max = TWO_BEAM_MAX;
        sprintf(msg, "\n\nSIM Mode: TWO BEAM\n");
    } else if(simMode == THREE_BEAM) {
        phase_max = THREE_BEAM_MAX;
        sprintf(msg, "\n\nSIM Mode: THREE BEAM\n");           
    } else if (simMode == NO_SIM_Z_ONLY){
        phase_max = NO_BEAM_MAX;
        sprintf(msg, "\n\nSIM Mode: Z-Only\n");
    } else if (simMode == SINGLE_ANGLE) {
        phase_max = SINGLE_ANGLE_MAX;
        sprintf(msg, "\n\nSIM Mode: SINGLE ANGLE\n");
    } else {
        phase_max = NO_BEAM_MAX;
        simMode = NO_SIM_Z_ONLY;
        sprintf(msg, "\n\nSIM Mode: Z-Only\n");
    }
    /*if(mode == 0){
        sprintf(line0, "Mode: Free FPS: %.1f", fps_in);
    } else if(mode == 1){
        sprintf(line0, "Mode: Z-St FPS: %.1f", fps_in);
    } else {
       sprintf(line0, "Mode: Timed FPS: %.1f", fps_in);   
    }
    
    while (0u == USBUART_CDCIsReady())
    {
    }
    USBUART_PutData((uint8*)msg, strlen(msg));
    
    LCD_Char_Position(0u, 0u);
    LCD_Char_PrintString(line0);*/
    
}


void set_laser_mode(uint8 laser_mode){
    laser_conf = laser_mode;   
    if(laser_conf == BLUE_LASER){
        CAM_SEL_REG_Write(BLUE_LASER);
        if(capMode == CAP_MODE_NORMAL){
            horzPeriod = ANDOR_30_MHZ_HORZ;
            if(capMode == CAP_MODE_NORMAL){
                BLANKING_DELAY_WritePeriod(ANDOR_DELAY_TICKS);
            } else {
                BLANKING_DELAY_WritePeriod(ANDOR_DELAY_TICKS);    
            }
        } else {
            horzPeriod = ANDOR_30_MHZ_HORZ;
            BLANKING_DELAY_WritePeriod(ANDOR_DELAY_TICKS);
        }
    } else if(laser_conf == GREEN_LASER){
        CAM_SEL_REG_Write(GREEN_LASER);
        BLANKING_DELAY_WritePeriod(ANDOR_DELAY_TICKS);               
    } else {
        // Try Set Readout Time to 0
        //horzPeriod = 0;
        CAM_SEL_REG_Write(GREEN_LASER);
        BLANKING_DELAY_WritePeriod(ANDOR_DELAY_TICKS);    
    }
    setExposure();    
}


void set_capMode(uint32 buf){
    if(buf & SLOW_READOUT){
        capMode = CAP_MODE_SLOW;   
    } else {
        capMode = CAP_MODE_NORMAL;   
    }
}

void setExposure(void){
    readTime();
    exposureMax = 1 / fps_in - readOutTime - (double)(SLM_CNTR_TICKS + SLM_TRG_TICKS) * COUNT_PERIOD;
    if(exposureMax < 0){
        if(capMode == CAP_MODE_NORMAL){
            exposureMax = 1 / TEN - readOutTime - (double)(SLM_CNTR_TICKS + SLM_TRG_TICKS) * COUNT_PERIOD;
            fps_in = TEN;
            //sprintf(msg, "\n***Timing error setting to 10fps***\n");
            //while (0u == USBUART_CDCIsReady())
            //{
            //}
            /* Send Messege */
            //USBUART_PutData((uint8*)msg, strlen(msg));
        } else {
            exposureMax = 1 / TEN - readOutTime - (double)(SLM_CNTR_TICKS + SLM_TRG_TICKS) * COUNT_PERIOD;
            fps_in = TEN;
            //sprintf(msg, "\n***Timing error setting to 10fps***\n");
            //while (0u == USBUART_CDCIsReady())
            //{
            //}
            /* Send Messege */
            //USBUART_PutData((uint8*)msg, strlen(msg));
        }
        
        if(laser_conf == GREEN_LASER){
            exposureMax = 1 / TEN - readOutTime - (double)(SLM_CNTR_TICKS + SLM_TRG_TICKS) * COUNT_PERIOD;
            fps_in = TEN;
            //sprintf(msg, "\n***Timing error setting to 10fps***\n");
            //while (0u == USBUART_CDCIsReady())
            //{
            //}
            /* Send Messege */
            //USBUART_PutData((uint8*)msg, strlen(msg));               
        } else if (laser_conf == BOTH_LASERS){
            exposureMax = 1 / FOURTY_EIGHT - readOutTime - (double)(SLM_CNTR_TICKS + SLM_TRG_TICKS) * COUNT_PERIOD;
            fps_in = FOURTY_EIGHT;
            //sprintf(msg, "\n***Timing error setting to 48fps***\n");
            //while (0u == USBUART_CDCIsReady())
            //{
            //}
            /* Send Messege */
            //USBUART_PutData((uint8*)msg, strlen(msg)); 
        }
        
    }
    

    
    exposure = exposureMax;
    outgoing.exposure = exposure;
    outgoing.flags |= SET_EXPOSURE;
    exposureTicks = (uint32)(exposure/COUNT_PERIOD);
    TRG_CNT_WritePeriod(exposureTicks);
    TRIG_CNT_RST_Write(REG_ON);
    TRIG_CNT_RST_Write(REG_OFF);
    setWaitTime();
    
}

void userSetExposure(void){
    //readTime();
    //exposureMax = 1 / fps_in - readOutTime - (double)(SLM_CNTR_TICKS + SLM_TRG_TICKS) * COUNT_PERIOD;
    //if(exposureMax < exposure){
        //exposure = exposureMax;
        //sprintf(msg, "\n\nExposure too long setting to exposureMax\n");
        //while (0u == USBUART_CDCIsReady())
        //{
        //}
        /* Send Messege */
        //USBUART_PutData((uint8*)msg, strlen(msg));
    //}
    fps_in = 1/(exposure + readOutTime - ((double)SLM_CNTR_TICKS)*COUNT_PERIOD/2);
    outgoing.fps = fps_in;
    outgoing.flags |= CHANGE_FPS;
    exposureTicks = (uint32)(exposure/COUNT_PERIOD);
    TRG_CNT_WritePeriod(exposureTicks);
    TRIG_CNT_RST_Write(REG_ON);
    TRIG_CNT_RST_Write(REG_OFF);
    setWaitTime();
    
}

void setWaitTime(void){
    double float_ticks = (readOutTime / COUNT_PERIOD) - (double)SLM_CNTR_TICKS / 2.0f;
    
    if(float_ticks <= 0 /*|| laser_conf == BOTH_LASERS*/){
        wait_time_ticks = SLM_CNTR_TICKS + STUPID;
    } else {
        wait_time_ticks = (uint32)(readOutTime / COUNT_PERIOD) - SLM_TRG_TICKS / 2 + STUPID;
    }
    
    //if(laser_conf == BOTH_LASERS){
        uint32 remain_ticks = frameTicks - exposureTicks - SLM_TRG_TICKS;
        if(wait_time_ticks < remain_ticks){
             wait_time_ticks = remain_ticks + STUPID;   
        }
        if(wait_time_ticks < SLM_CNTR_TICKS){
            wait_time_ticks = SLM_CNTR_TICKS + STUPID;
        }
    //}
    //sprintf(msg, "\nwait_time_ticks: %lu\nexposure: %.6f\n", wait_time_ticks, exposure);
    //while (0u == USBUART_CDCIsReady())
    //{
    //}
    /* Send Messege */
    //USBUART_PutData((uint8*)msg, strlen(msg));
}

void readTime(void){
    //readOutTime = (vert/2.0 + 10)*horzPeriod;
    readOutTime = (vert + 10)*horzPeriod + 0.0064;// 64 is a fudge to see if we can obey andor readout timing
}

/* [] END OF FILE */
