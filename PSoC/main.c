#include "project.h"
#include "ssd1306.h"
#include <stdio.h>

typedef enum
{
    SENSOR_SCAN = 0x01u,    /* Sensor is scanned in this state */
    WAIT_FOR_SCAN_COMPLETE = 0x02u, /* CPU is put to sleep in this state */
    PROCESS_DATA = 0x03u,   /* Sensor data is processed */
} DEVICE_STATE;

typedef enum
{
    Off,
    On,
    Recording,
    Disconnected
} CAM_STATE;

const char* StateArray[] = {"Off", "On", "Recording", "Disconnected"};

int main(void)
{
    char buffer [30];
    uint8 rdBuf[1], wrBuf[4], BattLevel = 0;
    uint32 interruptState = 0, byteCnt = 0, LoopCount = 0, PrevLoopCount = 0;
    DEVICE_STATE currentState = SENSOR_SCAN;
    CAM_STATE GoProState = Disconnected;
    
    CyGlobalIntEnable;
    Counter_Start();
    CapSense_Start();
    CapSense_InitializeWidgetBaseline(0);
    I2C_Start();
    display_init(0x3C);
    display_clear();
    display_update();
    gfx_setTextColor(WHITE);
    gfx_setTextSize(2);

    for(;;)
    {
        switch(currentState)
        {
            case SENSOR_SCAN:
                if(CapSense_IsBusy() == CapSense_NOT_BUSY)
                {
                    CapSense_RunTuner();      
                    CapSense_ScanAllWidgets();
                    currentState = WAIT_FOR_SCAN_COMPLETE;
                }
            	break;
			case WAIT_FOR_SCAN_COMPLETE:
                interruptState = CyEnterCriticalSection();
                CapSense_IsBusy() == CapSense_NOT_BUSY ? (currentState = PROCESS_DATA) : CySysPmSleep();
                CyExitCriticalSection(interruptState);
            	break;
			case PROCESS_DATA:
                currentState = SENSOR_SCAN;
                CapSense_ProcessAllWidgets();
                byteCnt = 0;
                if(CapSense_IsWidgetActive(0) && LoopCount - PrevLoopCount > 1)
                {
                    PrevLoopCount = LoopCount;
                    switch(GoProState)
                    {
                        case Disconnected:
                        case Off:
                            if (CapSense_IsSensorActive(0, 0))
                            {
                                GoProState = On;
                                wrBuf[0]=0x0F;
                                byteCnt=1;
                            }                        
                            break;
                        case On:
                            if (CapSense_IsSensorActive(0, 0))
                            {
                                GoProState = Recording;
                                wrBuf[0]=0x03;
                                wrBuf[1]=0x01;
                                wrBuf[2]=0x01;
                                wrBuf[3]=0x01;
                                byteCnt=4;
                            } 
                            if (CapSense_IsSensorActive(0, 1))
                            {
                                GoProState = Off;
                                wrBuf[0]=0x01;
                                wrBuf[1]=0x05;
                                byteCnt=2;
                            }
                            break;
                        case Recording:
                            if (CapSense_IsSensorActive(0, 1))
                            {
                                GoProState = On;
                                wrBuf[0]=0x03;
                                wrBuf[1]=0x01;
                                wrBuf[2]=0x01;
                                wrBuf[3]=0x00;
                                byteCnt=4;
                            }                        
                            break;
                        default:
            	           	break;
                    }
                }
                I2C_I2CMasterClearStatus();
                if (byteCnt > 0) I2C_I2CMasterWriteBuf(0x09, wrBuf, byteCnt, I2C_I2C_MODE_COMPLETE_XFER);
                else if (Counter_ReadCounter() > Counter_ReadPeriod() - 100)
                {
                    I2C_I2CMasterReadBuf(0x09, rdBuf, 1, I2C_I2C_NAK_DATA);
                    BattLevel = (rdBuf[0] > 0 && rdBuf[0] < 101)? rdBuf[0] : BattLevel;
                    snprintf(buffer, sizeof(buffer), "GoPro\r\n%s", StateArray[GoProState]);
                    if (BattLevel > 2) snprintf(buffer, sizeof(buffer), "%s\r\n\r\nBatt. %d%%", buffer, BattLevel);                    
                    display_clear();
                    gfx_setCursor(0,0);
                    gfx_println(buffer);
                    display_update();
                    LoopCount++;
                }
            	break;
            default:
            	CYASSERT(0);
            	break;
        }
    }
}