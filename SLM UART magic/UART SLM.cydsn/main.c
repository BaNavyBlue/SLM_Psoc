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
#define USBUART_BUFFER_SIZE (64u)
#define LINE_STR_LENGTH     (20u)

#define COUNT_PERIOD (0.0000005f) // 2MHz counter ticks 500ns
#define SLM_CNTR_TICKS (5360u)    // 1.18ms + 1.5ms
#define SLM_TRG_TICKS (200u)      // 50us
//#define ANDOR_READ_TIME (2000u)   // 1MHz worst case is 1ms
#define THIRTY_FPS (66667u)       // 33.3ms
#define THIRTY_EXP (40955u)       // 33.3ms - 1ms - 1.18ms * 2
#define THIRTY (30.0f)
#define TWENTY_SIX_FPS (76923u)
#define TWENTY_SIX (26.0f)
#define STAGE_TICKS (180000u)     // Xms for stage trigger
//#define STAGE_TICKS (12000u)
#define HAMA_SLOW_TICKS (650u)
#define HAMA_NORM_TICKS (195u)
#define HAMA_SLOW_READ (59586u)//(80920)//(58920u)  // 33ms - 1.18ms * 3
#define HAMA_SLOW_HORZ (.0000324812f) 
#define HAMA_NORM_HORZ (.00000974436f)
#define HAMA_VERT_DEFAULT (2048u)
#define CAP_MODE_NORMAL (0u)
#define CAP_MODE_SLOW (1u)

#define THREE_BEAM (0u)
#define TWO_BEAM (1u)
#define NO_SIM_Z_ONLY (2u)
#define THREE_BEAM_MAX (15u)
#define TWO_BEAM_MAX (9u)
#define NO_BEAM_MAX (1u)

#define BLANK_ON (0u)
#define BLANK_OFF (1u)

#define ENTR_FRAME_RATE "\n\nEnter FPS float: "
#define ENTR_Z_STEPS "\n\nEnter Z-Steps: "
#define ENTER_TIME_SECS "\n\nEnter Time(secs) float: "
#define ENTER_VERT_VALUE "\n\nEnter Vertical Pixels: "
#define ENTER_CAP_MODE "\n\nSet Capture Mode:\n 0) Normal\n 1) Slow\nEnter Choice: "
#define SELECT_MODE "\n\nSet Mode:\n 0) Free Run\n 1) Z-mode\n 2) Timed\nEnter Choice: "
#define SIM_SELECT "\n\nSet SIM Mode:\n 0) Three beam\n 1) Two beam\n "
#define SIM_SELECT2 "2) Z only\nEnter Choice: "
#define SET_EXPOSURE "\n\nEnter Exposure float: "
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

char8* parity[] = {"None", "Odd", "Even", "Mark", "Space"};
char8* stop[]   = {"1", "1.5", "2"};


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
uint16 vert = HAMA_VERT_DEFAULT;
uint8 capMode = CAP_MODE_NORMAL;
double horzPeriod = HAMA_NORM_HORZ;
double readOutTime = 0;
volatile uint8 mode = FREE_RUN;
volatile uint8 simMode = THREE_BEAM;
volatile uint16 zSteps = 0;
volatile uint32 frameTicks = THIRTY_FPS;
volatile uint32 exposureTicks = THIRTY_EXP;
volatile float exposureSeconds = 0.0333f;
volatile double exposure = 0.0333f;
volatile double exposureMax = 0.0333f;
volatile uint32 time_ticks_rem = 0;
uint32 time_ticks = 0;
uint32 wait_time_ticks = 0;
volatile uint8 msgFlag = 0;
volatile uint8 phase_max = THREE_BEAM_MAX;
volatile uint8 laser_conf = BLUE_LASER;


char line0[20];
char line1[20];
char line2[20];
char line3[20];

double fps_in = THIRTY;


char msg[64];

CY_ISR(T_ISR){
    
    // Controls the trigger periods and
    // which device gets the trigger pulse
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
            TRG_CNT_WritePeriod(exposureTicks);

            TRIG_CNT_RST_Write(REG_ON);
            TRIG_CNT_RST_Write(REG_OFF);
 

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
                
                STAGE_REG_Write(REG_ON);
                STAGE_REG_Write(REG_OFF);
                zCount++;
            } else {
                ENBL_TRIG_ISR_Write(REG_OFF); 
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
void read_input(uint8* buf, const char cmd);
void set_mode(uint8* buf);
void set_sim_mode(uint8* buf);
void set_laser_mode(uint8* buf);
void set_capMode(uint8* buf);
void setExposure(void);
void userSetExposure(void);
void setWaitTime(void);
void readTime(void);

int main()
{
    uint16 count;
    uint8 buffer[USBUART_BUFFER_SIZE] = {0};
    
#if (CY_PSOC3 || CY_PSOC5LP)
    uint8 state;
    char8 lineStr[LINE_STR_LENGTH];
    
    LCD_Char_Start();
#endif /* (CY_PSOC3 || CY_PSOC5LP) */
    
    CyGlobalIntEnable;

    //PWM_Start();  No more PWM
    TRG_CNT_Start();
    SLM_WAIT_Start();
    SLM_TRIG_Start();
    BLANKING_DELAY_Start();
    STAGE_TRIG_Start();
    TRIG_ISR_StartEx(T_ISR);

    /* Start USBFS operation with 5-V operation. */
    USBUART_Start(USBFS_DEVICE, USBUART_5V_OPERATION);
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
    SLM_WAIT_WritePeriod(wait_time_ticks);
    TRIG_CNT_RST_Write(REG_ON);
    ACTIVATE_SLM_Write(REG_ON);
    STAGE_REG_Write(REG_ON);
    CyDelay(100);
    TRIG_CNT_RST_Write(REG_OFF);
    ACTIVATE_SLM_Write(REG_OFF);
    STAGE_REG_Write(REG_OFF);
    BLANKING_DELAY_WritePeriod(HAMA_NORM_TICKS);
    BLANK_TOGGLE_Write(BLANK_OFF);
    
    LCD_Char_Position(0u, 0u);
    LCD_Char_PrintString("Mode: Free");
    LCD_Char_Position(0u, 11u);
    LCD_Char_PrintString("FPS: 30.0");
    LCD_Char_Position(1u, 0u);
    LCD_Char_PrintString(TRIGG_OFF);
    LCD_Char_Position(1u, 10u);
    LCD_Char_PrintString("Blank: Off");
    
    for(;;)
    {
        /* Host can send double SET_INTERFACE request. */
        if (0u != USBUART_IsConfigurationChanged())
        {
            /* Initialize IN endpoints when device is configured. */
            if (0u != USBUART_GetConfiguration())
            {
                /* Enumeration is done, enable OUT endpoint to receive data 
                 * from host. */
                USBUART_CDC_Init();
            }
            CyDelay(500);
            strcpy(msg, OPTIONS);
            /* Send  OPTIONS. */
            /* Wait until component is ready to send data to host. */
            while (0u == USBUART_CDCIsReady())
            {
            }
            USBUART_PutData((uint8*)msg, strlen(msg));
            /* Wait until component is ready to send data to host. */
            while (0u == USBUART_CDCIsReady())
            {
            }
            strcpy(msg, OPTIONS2);
            USBUART_PutData((uint8*)msg, strlen(msg));
            /* Wait until component is ready to send data to host. */
            while (0u == USBUART_CDCIsReady())
            {
            }
            strcpy(msg, OPTIONS3);
            USBUART_PutData((uint8*)msg, strlen(msg)); 
            /* Wait until component is ready to send data to host. */
            while (0u == USBUART_CDCIsReady())
            {
            }
            strcpy(msg, OPTIONS4);
            USBUART_PutData((uint8*)msg, strlen(msg));
            /* Wait until component is ready to send data to host. */
            while (0u == USBUART_CDCIsReady())
            {
            }
            strcpy(msg, OPTIONS5);
            USBUART_PutData((uint8*)msg, strlen(msg));     


        }

        /* Service USB CDC when device is configured. */
        if (0u != USBUART_GetConfiguration())
        {
            /* Check for input data from host. */
            if (0u != USBUART_DataIsReady())
            {
                /* Read received data and re-enable OUT endpoint. */
                count = USBUART_GetAll(buffer);

                if (0u != count)
                {
                    /* Wait until component is ready to send data to host. */
                    while (0u == USBUART_CDCIsReady())
                    {
                    }

                    /* Send data back to host. */
                    USBUART_PutData(buffer, count);

                    /* If the last sent packet is exactly the maximum packet 
                    *  size, it is followed by a zero-length packet to assure
                    *  that the end of the segment is properly identified by 
                    *  the terminal.
                    */
                    if (USBUART_BUFFER_SIZE == count)
                    {
                        /* Wait until component is ready to send data to PC. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        /* Send zero-length packet to PC. */
                        USBUART_PutData(NULL, 0u);
                    }
                }
                
                switch ((char)buffer[0]) {
                    case 'a':
                        strcpy(msg, ENTR_FRAME_RATE);
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        /* Send MSG back to host. */
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        read_input(buffer, 'a');
                        SLM_WAIT_WritePeriod(wait_time_ticks);
                        break;
                    case 'b':
                        strcpy(msg, ENTR_Z_STEPS);
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        /* Send MSG back to host. */
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        read_input(buffer, 'b');
                        break;
                    case 'c':
                        strcpy(msg, ENTER_TIME_SECS);
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        /* Send MSG back to host. */
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        read_input(buffer, 'c');
                        break;
                    case 'd':
                        strcpy(msg, ENTER_VERT_VALUE);
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        /* Send MSG back to host. */
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        read_input(buffer, 'd');
                        readTime();
                        SLM_WAIT_WritePeriod(wait_time_ticks);
                        break;
                    case 'e':
                        strcpy(msg, ENTER_CAP_MODE);
                        /* Send MSG back to host. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        set_capMode(buffer);
                        if(capMode == CAP_MODE_NORMAL){
                            horzPeriod = HAMA_NORM_HORZ;
                            BLANKING_DELAY_WritePeriod(HAMA_NORM_TICKS);
                        } else {
                            horzPeriod = HAMA_SLOW_HORZ;
                            BLANKING_DELAY_WritePeriod(HAMA_SLOW_TICKS);
                        }
                        setExposure();
                        SLM_WAIT_WritePeriod(wait_time_ticks);
                        break;
                    case 'f':
                        strcpy(msg, SELECT_MODE);
                        /* Send MSG back to host. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        set_mode(buffer);
                        break;
                    case 'g':
                        strcpy(msg, "\n\n****Starting Capture****\n\n");
                        /* Send MSG back to host. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
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
                        break;
                    case 'h':
                        strcpy(msg, "\n\n****Ending Capture****\n\n");
                        /* Send MSG back to host. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        ENBL_TRIG_ISR_Write(REG_OFF);
                        LCD_Char_Position(1u,0u);
                        LCD_Char_PrintString(TRIGG_OFF);
                        break;
                    case 'i':
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
                        
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        break;
                    case 'j':
                        strcpy(msg, SIM_SELECT);
                        /* Send MSG back to host. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        strcpy(msg, SIM_SELECT2);
                        /* Send MSG back to host. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));                        
                        set_sim_mode(buffer);                        
                        break;
                    case 'k':
                        if(ENBL_TRIG_ISR_Read()){
                            sprintf(msg, "\n\nTrigger: Running\n");   
                        } else {
                            sprintf(msg, "\n\nTrigger: Stopped\n");                             
                        }
                        
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));                        
                        
                        if(capMode == CAP_MODE_NORMAL){
                            sprintf(msg, "Camera Readout Mode: Normal\n");
                        } else {
                             sprintf(msg, "Camera Readout Mode: SLOW\n");   
                        }
                        /* Send  OPTIONS. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        if(mode == 0){
                            sprintf(msg, "Mode: Free Run\n");
                        } else if (mode == 1){
                            sprintf(msg, "Mode: Z-Stack\n");
                        } else {
                            sprintf(msg, "Mode: Timed\n");
                        }
                        
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        if(simMode == 0){
                            sprintf(msg, "SIM Mode: Three Beam\n");
                        } else if(simMode == TWO_BEAM) {
                            sprintf(msg, "SIM Mode: Two Beam\n");    
                        } else {
                            sprintf(msg, "SIM Mode: Z-only\n");   
                        }
                        
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        if(BLANK_TOGGLE_Read()){
                            sprintf(msg, "Blanking: Off\n");
                        } else {
                            sprintf(msg, "Blanking: On\n");
                        }
                        
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));                        
                        
                        sprintf(msg, "FPS: %.3f\n", fps_in);
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        sprintf(msg, "exposure time: %.6f (sec)\n", exposure);                        
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        sprintf(msg, "Z-steps: %u\n", zSteps);
                        
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));

                        if(laser_conf == BLUE_LASER){
                            sprintf(msg, "Blue Laser\n");  
                        } else if (laser_conf == GREEN_LASER){
                            sprintf(msg, "Green Laser\n");
                        } else {
                            sprintf(msg, "Both Lasers Alternateing\n");   
                        }
                        
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        break;
                    case 'l':
                        sprintf(msg, "\n\nCan not exceed %.6f (sec) exposure to maintain fps\n", exposureMax);
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        strcpy(msg, SET_EXPOSURE);
                        /* Send MSG back to host. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        read_input(buffer,'l');
                        SLM_WAIT_WritePeriod(wait_time_ticks);
                        break;                        
                    case 'm':
                        strcpy(msg, LASER_SELECT);
                        /* Send MSG back to host. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        strcpy(msg, LASER_SELECT2);
                        /* Send MSG back to host. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        set_laser_mode(buffer);
                        break;
                    case '\n':
                        strcpy(msg, OPTIONS);
                        /* Send  OPTIONS. */
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        strcpy(msg, OPTIONS2);
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        strcpy(msg, OPTIONS3);
                        USBUART_PutData((uint8*)msg, strlen(msg));
                        
                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        strcpy(msg, OPTIONS4);
                        USBUART_PutData((uint8*)msg, strlen(msg)); 

                        /* Wait until component is ready to send data to host. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        strcpy(msg, OPTIONS5);
                        USBUART_PutData((uint8*)msg, strlen(msg));

                        
                        break;
                    default:
                        break;
                }
                
            }


        #if (CY_PSOC3 || CY_PSOC5LP)
            /* Check for Line settings change. */
            state = USBUART_IsLineChanged();
            if (0u != state)
            {
                /* Output on LCD Line Coding settings. */
                if (0u != (state & USBUART_LINE_CODING_CHANGED))
                {                  
                    /* Get string to output. */
                    sprintf(lineStr,"BR:%4ld %d%c%s", USBUART_GetDTERate(),\
                                   (uint16) USBUART_GetDataBits(),\
                                   parity[(uint16) USBUART_GetParityType()][0],\
                                   stop[(uint16) USBUART_GetCharFormat()]);

                    /* Clear LCD line. */
                    LCD_Char_Position(0u, 0u);
                    LCD_Char_PrintString("                    ");

                    /* Output string on LCD. */
                    LCD_Char_Position(0u, 0u);
                    LCD_Char_PrintString(lineStr);
                }

                /* Output on LCD Line Control settings. */
                if (0u != (state & USBUART_LINE_CONTROL_CHANGED))
                {                   
                    /* Get string to output. */
                    state = USBUART_GetLineControl();
                    sprintf(lineStr,"DTR:%s,RTS:%s",  (0u != (state & USBUART_LINE_CONTROL_DTR)) ? "ON" : "OFF",
                                                      (0u != (state & USBUART_LINE_CONTROL_RTS)) ? "ON" : "OFF");

                    /* Clear LCD line. */
                    LCD_Char_Position(1u, 0u);
                    LCD_Char_PrintString("                    ");

                    /* Output string on LCD. */
                    LCD_Char_Position(1u, 0u);
                    LCD_Char_PrintString(lineStr);
                }
            }
        #endif /* (CY_PSOC3 || CY_PSOC5LP) */
        }
        switch(msgFlag){
            case Z_FIN:
                sprintf(msg, "\n\n****Z-capture Finished****\n\n");
                /* Wait until component is ready to send data to PC. */
                while (0u == USBUART_CDCIsReady())
                {
                }
                USBUART_PutData((uint8*)msg, strlen(msg));
                msgFlag &= ~Z_FIN;
                break;
            case TIMED_FIN:
                sprintf(msg, "\n\n****Timed-capture Finished****\n\n");
                /* Wait until component is ready to send data to PC. */
                while (0u == USBUART_CDCIsReady())
                {
                }                
                USBUART_PutData((uint8*)msg, strlen(msg));
                msgFlag &= ~TIMED_FIN;
                break;
            default:
                break;
        }
    }
}

// UART MAGIC
void read_input(uint8* buf, const char cmd){
   char val_str[32] = {0};
   uint8 i = 0;
   uint8 count = 0;
   while(buf[0] != '\n'){
               /* Service USB CDC when device is configured. */
        if (0u != USBUART_GetConfiguration())
        {
            /* Check for input data from host. */
            if (0u != USBUART_DataIsReady())
            {
                /* Read received data and re-enable OUT endpoint. */
                count = USBUART_GetAll(buf);

                if (0u != count)
                {
                    /* Wait until component is ready to send data to host. */
                    while (0u == USBUART_CDCIsReady())
                    {
                    }

                    /* Send data back to host. */
                    if ( (buf[0] >= '0' && buf[0] <= '9')|| buf[0] == '.'){
                        USBUART_PutData(buf, count);
                        // append value to float string
                        val_str[i++] = buf[0];                        
                    }

                    /* If the last sent packet is exactly the maximum packet 
                    *  size, it is followed by a zero-length packet to assure
                    *  that the end of the segment is properly identified by 
                    *  the terminal.
                    */
                    if (USBUART_BUFFER_SIZE == count)
                    {
                        /* Wait until component is ready to send data to PC. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        /* Send zero-length packet to PC. */
                        USBUART_PutData(NULL, 0u);
                    }
                }
            }
        }
   }
    double time_s;
    if(i < 31){
        val_str[i] = '\0';
        switch(cmd){
            case 'a':
                fps_in = strtof(val_str, NULL);
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
                zSteps = (uint16)atoi(val_str);
                sprintf(msg, "\n\nSetting: %s, zSteps: %u\n\n", val_str, zSteps);
                break;
            case 'c':
                time_s = strtof(val_str, NULL);
                time_ticks = (uint32)(time_s / COUNT_PERIOD);
                sprintf(msg, "\n\nSetting: %s, time_ticks: %lu\n\n", val_str, time_ticks);
                break;
            case 'd':
                vert = (uint16)atoi(val_str);
                setExposure();
                sprintf(msg, "\n\nSetting: %s, exposure_ticks: %lu\n\n", val_str, exposureTicks);
                break;
            case 'l':
                exposure = strtof(val_str, NULL);
                userSetExposure();
                sprintf(msg, "\n\nSetting exposure: %.6f, exposureTicks: %lu\n\n", exposure, exposureTicks);
                break;
            default:
                sprintf(msg, "\n\n****something wrong has happened****\n\n");
                break;
        }
        while (0u == USBUART_CDCIsReady())
        {
        }
        USBUART_PutData((uint8*)msg, strlen(msg));
    } else {
        sprintf(msg, "\n\n****number too long****\n\n");
        USBUART_PutData((uint8*)msg, strlen(msg));
    }
}

void set_mode(uint8* buf){
   char val_str[32] = {0};
   uint8 i = 0;
   uint8 count = 0;
   while(i < 1){
               /* Service USB CDC when device is configured. */
        if (0u != USBUART_GetConfiguration())
        {
            /* Check for input data from host. */
            if (0u != USBUART_DataIsReady())
            {
                /* Read received data and re-enable OUT endpoint. */
                count = USBUART_GetAll(buf);

                if (0u != count)
                {
                    /* Wait until component is ready to send data to host. */
                    while (0u == USBUART_CDCIsReady())
                    {
                    }

                    /* Send data back to host. */
                    if ( (buf[0] >= '0' && buf[0] <= '2')|| buf[0] == '\n'){
                        USBUART_PutData(buf, count);
                        // append value to float string
                        val_str[i++] = buf[0];                        
                    }

                    /* If the last sent packet is exactly the maximum packet 
                    *  size, it is followed by a zero-length packet to assure
                    *  that the end of the segment is properly identified by 
                    *  the terminal.
                    */
                    if (USBUART_BUFFER_SIZE == count)
                    {
                        /* Wait until component is ready to send data to PC. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        /* Send zero-length packet to PC. */
                        USBUART_PutData(NULL, 0u);
                    }
                }
            }
        }
    }
    if(val_str[0] != '\n'){
        mode = (uint8)atoi(val_str);   
    }
    sprintf(msg, "\n\nMode: %u\n", mode);
    while (0u == USBUART_CDCIsReady())
    {
    }
    USBUART_PutData((uint8*)msg, strlen(msg));
    if(mode == 0){
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
    LCD_Char_PrintString(line0);
    
}

void set_sim_mode(uint8* buf){
   char val_str[32] = {0};
   uint8 i = 0;
   uint8 count = 0;
   while(i < 1){
               /* Service USB CDC when device is configured. */
        if (0u != USBUART_GetConfiguration())
        {
            /* Check for input data from host. */
            if (0u != USBUART_DataIsReady())
            {
                /* Read received data and re-enable OUT endpoint. */
                count = USBUART_GetAll(buf);

                if (0u != count)
                {
                    /* Wait until component is ready to send data to host. */
                    while (0u == USBUART_CDCIsReady())
                    {
                    }

                    /* Send data back to host. */
                    if ( (buf[0] >= '0' && buf[0] <= '2')|| buf[0] == '\n'){
                        USBUART_PutData(buf, count);
                        // append value to float string
                        val_str[i++] = buf[0];                        
                    }

                    /* If the last sent packet is exactly the maximum packet 
                    *  size, it is followed by a zero-length packet to assure
                    *  that the end of the segment is properly identified by 
                    *  the terminal.
                    */
                    if (USBUART_BUFFER_SIZE == count)
                    {
                        /* Wait until component is ready to send data to PC. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        /* Send zero-length packet to PC. */
                        USBUART_PutData(NULL, 0u);
                    }
                }
            }
        }
    }
    if(val_str[0] != '\n'){
        simMode = (uint8)atoi(val_str);   
    }
    if(simMode == TWO_BEAM){
        phase_max = TWO_BEAM_MAX;
        sprintf(msg, "\n\nSIM Mode: TWO BEAM\n");
    } else if(simMode == THREE_BEAM) {
        phase_max = THREE_BEAM_MAX;
        sprintf(msg, "\n\nSIM Mode: THREE BEAM\n");           
    } else {
        phase_max = NO_BEAM_MAX;
        sprintf(msg, "\n\nSIM Mode: Z-Only\n");
    }

    while (0u == USBUART_CDCIsReady())
    {
    }
    USBUART_PutData((uint8*)msg, strlen(msg));
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


void set_laser_mode(uint8* buf){
   char val_str[32] = {0};
   uint8 i = 0;
   uint8 count = 0;
   while(i < 1){
               /* Service USB CDC when device is configured. */
        if (0u != USBUART_GetConfiguration())
        {
            /* Check for input data from host. */
            if (0u != USBUART_DataIsReady())
            {
                /* Read received data and re-enable OUT endpoint. */
                count = USBUART_GetAll(buf);

                if (0u != count)
                {
                    /* Wait until component is ready to send data to host. */
                    while (0u == USBUART_CDCIsReady())
                    {
                    }

                    /* Send data back to host. */
                    if ( (buf[0] >= '0' && buf[0] <= '2')|| buf[0] == '\n'){
                        USBUART_PutData(buf, count);
                        // append value to float string
                        val_str[i++] = buf[0];                        
                    }

                    /* If the last sent packet is exactly the maximum packet 
                    *  size, it is followed by a zero-length packet to assure
                    *  that the end of the segment is properly identified by 
                    *  the terminal.
                    */
                    if (USBUART_BUFFER_SIZE == count)
                    {
                        /* Wait until component is ready to send data to PC. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        /* Send zero-length packet to PC. */
                        USBUART_PutData(NULL, 0u);
                    }
                }
            }
        }
    }
    if(val_str[0] != '\n'){
        laser_conf = (uint8)atoi(val_str);   
    }
    if(laser_conf == BLUE_LASER){
        CAM_SEL_REG_Write(BLUE_LASER);
        sprintf(msg, "\n\nLaser Mode: 488nm\n");
    } else if(laser_conf == BLUE_LASER){
        CAM_SEL_REG_Write(GREEN_LASER);
        sprintf(msg, "\n\nLaser Mode: 561nm\n");           
    } else {
        sprintf(msg, "\n\nLaser Mode: 488nm and 561nm Alternating\n");
        CAM_SEL_REG_Write(GREEN_LASER);
    }
    
    setWaitTime();

    while (0u == USBUART_CDCIsReady())
    {
    }
    USBUART_PutData((uint8*)msg, strlen(msg));
    
}


void set_capMode(uint8* buf){
   char val_str[32] = {0};
   uint8 i = 0;
   uint8 count = 0;
   while(i < 1){
               /* Service USB CDC when device is configured. */
        if (0u != USBUART_GetConfiguration())
        {
            /* Check for input data from host. */
            if (0u != USBUART_DataIsReady())
            {
                /* Read received data and re-enable OUT endpoint. */
                count = USBUART_GetAll(buf);

                if (0u != count)
                {
                    /* Wait until component is ready to send data to host. */
                    while (0u == USBUART_CDCIsReady())
                    {
                    }

                    /* Send data back to host. */
                    if ( (buf[0] >= '0' && buf[0] <= '1')|| buf[0] == '\n'){
                        USBUART_PutData(buf, count);
                        // append value to float string
                        val_str[i++] = buf[0];                        
                    }

                    /* If the last sent packet is exactly the maximum packet 
                    *  size, it is followed by a zero-length packet to assure
                    *  that the end of the segment is properly identified by 
                    *  the terminal.
                    */
                    if (USBUART_BUFFER_SIZE == count)
                    {
                        /* Wait until component is ready to send data to PC. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }
                        /* Send zero-length packet to PC. */
                        USBUART_PutData(NULL, 0u);
                    }
                }
            }
        }
    }
    if(val_str[0] != '\n'){
        capMode = (uint8)atoi(val_str);   
    }
    sprintf(msg, "\n\nCapture Mode: %u\n", capMode);
    while (0u == USBUART_CDCIsReady())
    {
    }
    USBUART_PutData((uint8*)msg, strlen(msg));
}

void setExposure(void){
    readTime();
    exposureMax = 1 / fps_in - readOutTime - (double)(SLM_CNTR_TICKS + SLM_TRG_TICKS) * COUNT_PERIOD;
    if(exposureMax < 0){
        if(capMode == CAP_MODE_NORMAL){
            exposureMax = 1 / THIRTY - readOutTime - (double)(SLM_CNTR_TICKS + SLM_TRG_TICKS) * COUNT_PERIOD;
            fps_in = THIRTY;
            sprintf(msg, "\n***Timing error setting to 30fps***\n");
            while (0u == USBUART_CDCIsReady())
            {
            }
            /* Send Messege */
            USBUART_PutData((uint8*)msg, strlen(msg));
        } else {
            exposureMax = 1 / TWENTY_SIX - readOutTime - (double)(SLM_CNTR_TICKS + SLM_TRG_TICKS) * COUNT_PERIOD;
            fps_in = TWENTY_SIX;
            sprintf(msg, "\n***Timing error setting to 26fps***\n");
            while (0u == USBUART_CDCIsReady())
            {
            }
            /* Send Messege */
            USBUART_PutData((uint8*)msg, strlen(msg));
        }
    }
    exposure = exposureMax;
    exposureTicks = (uint32)(exposure/COUNT_PERIOD);
    setWaitTime();
    
}

void userSetExposure(void){
    //readTime();
    //exposureMax = 1 / fps_in - readOutTime - (double)(SLM_CNTR_TICKS + SLM_TRG_TICKS) * COUNT_PERIOD;
    if(exposureMax < exposure){
        exposure = exposureMax;
        sprintf(msg, "\n\nExposure too long setting to exposureMax\n");
        while (0u == USBUART_CDCIsReady())
        {
        }
        /* Send Messege */
        USBUART_PutData((uint8*)msg, strlen(msg));
    }

    exposureTicks = (uint32)(exposure/COUNT_PERIOD);
    setWaitTime();
    
}

void setWaitTime(void){
    double float_ticks = (readOutTime / COUNT_PERIOD) - (double)SLM_CNTR_TICKS / 2.0f;
    
    if(float_ticks <= 0 /*|| laser_conf == BOTH_LASERS*/){
        wait_time_ticks = SLM_CNTR_TICKS;
    } else {
        wait_time_ticks = (uint32)(readOutTime / COUNT_PERIOD) - SLM_TRG_TICKS / 2;
    }
    
    //if(laser_conf == BOTH_LASERS){
        uint32 remain_ticks = frameTicks - exposureTicks - SLM_TRG_TICKS;
        if(wait_time_ticks < remain_ticks){
             wait_time_ticks = remain_ticks;   
        }
        if(wait_time_ticks < SLM_CNTR_TICKS){
            wait_time_ticks = SLM_CNTR_TICKS;
        }
    //}
    sprintf(msg, "\nwait_time_ticks: %lu\nexposure: %.6f\n", wait_time_ticks, exposure);
    while (0u == USBUART_CDCIsReady())
    {
    }
    /* Send Messege */
    USBUART_PutData((uint8*)msg, strlen(msg));
}

void readTime(void){
    readOutTime = (vert/2.0 + 10)*horzPeriod;
}

/* [] END OF FILE */
