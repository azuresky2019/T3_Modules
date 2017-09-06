#include <string.h>
#include "stm32f10x.h"
#include "usart.h"
#include "delay.h"
#include "led.h"
//#include "key.h"
#include "24cxx.h"
#include "spi.h"
#include "lcd.h"
#include "touch.h"
#include "flash.h"
#include "stmflash.h"
//#include "sdcard.h"
#include "mmc_sd.h"
#include "dma.h"
#include "vmalloc.h"
#include "ff.h"
#include "fattester.h"
#include "exfuns.h"
#include "enc28j60.h"
#include "timerx.h"
#include "uip.h"
#include "uip_arp.h"
#include "tapdev.h"
#include "usb_app.h"
//#include "ai.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "modbus.h"
#include "define.h"
#include "inputs.h"
#include "../output/output.h"
#include "dlmstp.h"
#include "rs485.h"
#include "datalink.h"
#include "config.h"
//#include "handlers.h"
#include "device.h"	
#include "registerlist.h"
#include "../filter/filter.h"
#include "../KEY/key.h"
#include "bacnet.h"
#include "t3-pt12.h" 
#include "read_pt.h"
#include "pt12_i2c.h"
#include "store.h"
#include "watchdog.h"
#include "rfm69.h"
#include "accelero_meter.h"
#include "air_flow.h"

#if (defined T38AI8AO6DO)||(defined T322AI)||(defined T36CTA)
void vSTORE_EEPTask(void *pvParameters ) ;
static void vINPUTSTask( void *pvParameters );
#endif
static void vLED0Task( void *pvParameters );
static void vCOMMTask( void *pvParameters );
//static void vUSBTask( void *pvParameters );

static void vNETTask( void *pvParameters );
#ifdef T38AI8AO6DO
void vKEYTask( void *pvParameters );
void vOUTPUTSTask( void *pvParameters );
#endif
#ifdef T36CTA
void vKEYTask( void *pvParameters );
void vOUTPUTSTask( void *pvParameters );
void vAcceleroTask( void *pvParameters);
void vRFMTask(void *pvParameters);
void vCHECKRFMTask(void *pvParameters);
void vAirFlowTask( void *pvParameters);
void vGetACTask( void *pvParameters);
#endif
#ifdef T3PT12
void vI2C_READ(void *pvParameters) ;
#endif
static void vMSTP_TASK(void *pvParameters ) ;
void uip_polling(void);

#define	BUF	((struct uip_eth_hdr *)&uip_buf[0])	
	
u8 update = 0 ;

u32 Instance = 0x0c;
uint8_t  PDUBuffer[MAX_APDU];

u8 global_key = KEY_NON;

static void debug_config(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
}

#if defined T36CTA
uint8_t t36ct_ver = T36CTA_REV1;
bool comSwitch = false;
u8 comRecevieFlag = 0;
u16 outdoorTempC;
u16 outdoorTempH;
u16 outdoorHum;
u16 outdoorLux;
u16 masterPollCount;
#endif

int main(void)
{
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x8008000);
	debug_config();
	//ram_test = 0 ;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD , ENABLE);
 	delay_init(72);	
//	uart1_init(38400);
//	modbus.baudrate = 38400 ;
	//KEY_Init();
	beeper_gpio_init();
	beeper_on();
	beeper_off();	
	#if defined T36CTA
	ACCELERO_IO_Init();
	ACCELERO_I2C_init();
	ACCELERO_Write_Data(0x2a, 0x01);
	if( 0x5a == ACCELERO_Read_Data(0x0d))
	{
		t36ct_ver = T36CTA_REV2;
	}
	#endif
	EEP_Dat_Init();
	mass_flash_init() ;
	//Lcd_Initial();
	SPI1_Init();
	SPI2_Init();
	watchdog_init();
//	mem_init(SRAMIN);
//	TIM3_Int_Init(5000, 7199);
//	TIM6_Int_Init(100, 7199);
//	TIM3_Int_Init(50, 7199);//5ms
	printf("T3_series\n\r");
	#if (defined T38AI8AO6DO)||(defined T322AI) ||(defined T36CTA)
		xTaskCreate( vINPUTSTask, ( signed portCHAR * ) "INPUTS", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL );
		xTaskCreate( vSTORE_EEPTask, ( signed portCHAR * ) "STOREEEP", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL );
	#endif
	xTaskCreate( vLED0Task, ( signed portCHAR * ) "LED0", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL );
 	xTaskCreate( vCOMMTask, ( signed portCHAR * ) "COMM", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL );
	xTaskCreate( vNETTask, ( signed portCHAR * ) "NET",  configMINIMAL_STACK_SIZE+256, NULL, tskIDLE_PRIORITY + 4, NULL );
//	xTaskCreate( vUSBTask, ( signed portCHAR * ) "USB", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL );
	//xTaskCreate( vUSBTask, ( signed portCHAR * ) "USB", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL );
	/* Start the scheduler. */
	#if (defined T38AI8AO6DO)
	xTaskCreate( vOUTPUTSTask, ( signed portCHAR * ) "OUTPUTS", configMINIMAL_STACK_SIZE+256, NULL, tskIDLE_PRIORITY + 2, NULL );
	#endif
	#if (defined T36CTA)
	xTaskCreate( vOUTPUTSTask, ( signed portCHAR * ) "OUTPUTS", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL );
	#endif
 	#if (defined T38AI8AO6DO) || (defined T36CTA)
	
	xTaskCreate( vKEYTask, ( signed portCHAR * ) "KEY", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL );
	#endif
	#if defined T36CTA
	xTaskCreate( vRFMTask, ( signed portCHAR * ) "RFM", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 4, NULL );
 	xTaskCreate( vAcceleroTask, ( signed portCHAR * ) "ACCELERO", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL );
	xTaskCreate( vGetACTask, ( signed portCHAR * ) "GETAC", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL );
		#if T36CTA_REV2
			xTaskCreate( vAirFlowTask, ( signed portCHAR * ) "AIRFLOW", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL );
		#endif
	#endif
	#ifdef T3PT12
	xTaskCreate( vI2C_READ, ( signed portCHAR * ) "READ_I2C", configMINIMAL_STACK_SIZE+256, NULL, tskIDLE_PRIORITY + 2, NULL );
	#endif
	#ifndef T36CTA
	xTaskCreate( vMSTP_TASK, ( signed portCHAR * ) "MSTP", configMINIMAL_STACK_SIZE + 256  , NULL, tskIDLE_PRIORITY + 4, NULL );
	#endif
//	#if defined T36CTA
//	xTaskCreate( vCHECKRFMTask, ( signed portCHAR * ) "CHECKRFM", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL );
//	#endif
	vTaskStartScheduler();
}
#if defined T36CTA

//uint8_t t36ct_ver = T36CTA_REV1;
uint32_t vol_sum[6];

void vGetACTask( void *pvParameters)
{
	static uint8 avg_count = 0;
	uint8 i;
	for(;;)
	{
		vol_buf[0][avg_count] = ADC_getChannal(ADC2,ADC_Channel_0);
		vol_buf[1][avg_count] = ADC_getChannal(ADC2,ADC_Channel_1);
		vol_buf[2][avg_count] = ADC_getChannal(ADC2,ADC_Channel_2);
		vol_buf[3][avg_count] = ADC_getChannal(ADC2,ADC_Channel_3);
		vol_buf[4][avg_count] = ADC_getChannal(ADC2,ADC_Channel_4);
		vol_buf[5][avg_count] = ADC_getChannal(ADC2,ADC_Channel_5);
		avg_count++;
		if(avg_count == (VOL_BUF_NUM-1))
		{
			vol_sum[0] = 0;
			vol_sum[1] = 0;
			vol_sum[2] = 0;
			vol_sum[3] = 0;
			vol_sum[4] = 0;
			vol_sum[5] = 0;
			
			for(i=0;i<avg_count;i++)
			{
				vol_sum[0]+=vol_buf[0][i];
				vol_sum[1]+=vol_buf[1][i];
				vol_sum[2]+=vol_buf[2][i];
				vol_sum[3]+=vol_buf[3][i];
				vol_sum[4]+=vol_buf[4][i];
				vol_sum[5]+=vol_buf[5][i];
			}
		}
		avg_count %= (VOL_BUF_NUM-1);
	
		delay_ms(10);
	}
}
void vAirFlowTask( void *pvParameters)
{

	#if T36CTA_REV2
	vUpdate_Pressure_Task(  pvParameters );
	#endif
}

extern u8 rfm69_tx_count;
extern bool rfm69_send_flag;
extern u8 rfm69_length;
extern u8 RFM69_SEND[];
bool rfm_exsit = false;
 bool test_print = false;
 uint16 acc_sensitivity[2];
extern uint8 acc_led_count;
u8 rfm69_checkData(u8 len)
{
   u16 crc_val;
   u8 i;

	//printf("rfm69_checkData\r\n");
   // check if talking to correct device ID
	if(len<3)
		return 0;
   if(rfm69_sendBuf[0] != 255 && rfm69_sendBuf[0] != modbus.address && rfm69_sendBuf[0] != 0)
      return 0;   


   // check that message is one of the following
   if( (rfm69_sendBuf[1]!=READ_VARIABLES) && (rfm69_sendBuf[1]!=WRITE_VARIABLES) && (rfm69_sendBuf[1]!=MULTIPLE_WRITE) )
      return 0;
   // ------------------------------------------------------------------------------------------------------
      // ------------------------------------------------------------------------------------------------------

//   if( len == 8)
//   {
//	   return 1;
//   }

 //  printf( "len = %d, %d, %d\r\n\r\n, ",rfm69_size, rfm69_sendBuf[len-2], rfm69_sendBuf[len-1]);
   crc_val = crc16(rfm69_sendBuf, len-2);//rfm69_size-2);


   if(crc_val == (rfm69_sendBuf[len-2]<<8) + rfm69_sendBuf[len-1] )
   {
	   return 1;
   }
   else
   {
      return 0;
   }
   //return 1;

 }


void vRFMTask( void *pvParameters)
{
	u8 temp,i;
	
	RFM69_GPIO_init();
	RFM69_TIMER_init();

	rfm_exsit = RFM69_initialize(0, RFM69_nodeID, 0);
	RFM69_encrypt(rfm69_key);
	RFM69_setBitRate(RFM69_biterate);

	RFM69_setMode(RF69_MODE_RX);
	for( ;; )
	{
		delay_ms(100);

		
		rfm69_deadMaster--;
		if(rfm69_deadMaster == 0)
		{
			{
				RFM69_freq = RFM69_getFrequency();
				if( ((RFM69_freq!= 915000000)&&(RFM69_freq!= 315000000)&&(RFM69_freq!= 433000000)&&(RFM69_freq!= 868000000)))
					//|| (RFM69_nodeID!= RFM69_getAddress()) ||(RFM69_getNetwork() != RFM69_networkID))
				{
					RFM69_freq = 915000000;
				}
				
				GPIO_SetBits(GPIOC,GPIO_Pin_12);
				delay_us(1000);
				GPIO_ResetBits(GPIOC,GPIO_Pin_12);
				delay_ms(100);
				RFM_CS = 0;
				SPI2_Init();
				RFM69_GPIO_init();
				rfm_exsit = RFM69_initialize(0, RFM69_nodeID, 0);
				RFM69_encrypt(rfm69_key);
				RFM69_setBitRate(RFM69_biterate);
				RFM69_setMode(RF69_MODE_RX);
				delay_ms(100);
			}
			rfm69_deadMaster = rfm69_set_deadMaster;
		}
		if(rfm69_send_flag)
		{
			rfm69_deadMaster = rfm69_set_deadMaster;
			//RFM69_sendWithRetry(rfm69_id, RFM69_SEND, rfm69_length, 0, 1);
			//printf("%d,%d,%d,%d,%d,%d,%d,%d\r\n\r\n\r\n", rfm69_sendBuf[0],rfm69_sendBuf[1],rfm69_sendBuf[2],rfm69_sendBuf[3],rfm69_sendBuf[4],rfm69_sendBuf[5],rfm69_sendBuf[6],rfm69_sendBuf[7]);
			if(rfm69_checkData(rfm69_size))//rfm69_sendBuf[0] == 255 || rfm69_sendBuf[0] == modbus.address || rfm69_sendBuf[0] == 0)
			{
				//test_print = true;
				init_crc16(); 
				responseCmd(10, rfm69_sendBuf);
				internalDeal(10, rfm69_sendBuf);
				RFM69_sendWithRetry(rfm69_id, RFM69_SEND, rfm69_length, RFM69_RETRIES, RFM69_RETRIES_TIMEOUT);
			}
			rfm69_send_flag = false;
		}

	}
}



extern u16 led_bank2;
void vAcceleroTask(void *pvParameters)
{
	uint16 acc_temp;
	int16 tempAcc;
	ACCELERO_IO_Init();
	/* Write CTL REG1 register, set ACTIVE mode */
 	ACCELERO_I2C_init();
	for( ;; )
	{
		ACCELERO_Write_Data(0x2a, 0x01);
		if( 0x5a == ACCELERO_Read_Data(0x0d))
		{
			t36ct_ver = T36CTA_REV2;
			//axis_value[0] = BUILD_UINT10_AXIS (ACCELERO_Read_Data(asix_sequence*2 + 0x01),ACCELERO_Read_Data(asix_sequence*2 + 0x02));
			acc_temp = BUILD_UINT10_AXIS (ACCELERO_Read_Data(asix_sequence*2 + 0x01),ACCELERO_Read_Data(asix_sequence*2 + 0x02));
			tempAcc = axis_value[asix_sequence] - acc_temp;
			axis_value[asix_sequence++] = acc_temp;
			asix_sequence %= 3;
		}
		if((ABS(tempAcc) > acc_sensitivity[0])&&(ABS(tempAcc) < acc_sensitivity[1]))
		{
			acc_led_count = 100;
			//led_bank2 &= ~(1<<3) ;
		}
		else
		{
			if(acc_led_count>0)
				acc_led_count--;
//			if(acc_led_count == 0)
//				led_bank2 |= (1<<3) ;
		}
		delay_ms(50);
	}
}
#endif
void vLED0Task( void *pvParameters )
{
	static u8 table_led_count =0 ;
	
	LED_Init();
	for( ;; )
	{
		table_led_count++ ;
		if(table_led_count>= 2)
		{
			table_led_count= 0 ;
			tabulate_LED_STATE();
		}
		
		delay_ms(10);
	}
}
void vCOMMTask(void *pvParameters )
{
//	uint16_t ram_left = 0 ; 
	u8  sendbuf[8];
	u16 sendCount = 0;
	modbus_init();
	
	for( ;; )
	{
		#if T36CTA
		if( comSwitch)
		{
			sendbuf[0] = 0xff;
			sendbuf[1] = 0x03;
			sendbuf[2] = 0x00;
			sendbuf[3] = 0x64;
			sendbuf[4] = 0x00;
			sendbuf[5] = 0x02;
			sendbuf[6] = 0x90;
			sendbuf[7] = 0x0a;
			if( sendCount == masterPollCount)
			{
				memcpy(uart_send, sendbuf, 8);
				TXEN = SEND;
				USART_SendDataString(8);
				comRecevieFlag = 0;
			}
			else if( sendCount == masterPollCount*2)
			{
				sendbuf[2] = 0x01;
				sendbuf[3] = 0x30;
				sendbuf[5] = 0x02;
				sendbuf[6] = 0xd0;
				sendbuf[7] = 0x26;
				memcpy(uart_send, sendbuf, 8);
				TXEN = SEND;
				USART_SendDataString(8);
				comRecevieFlag = 1;
			}
			else if( sendCount == masterPollCount*3)
			{
				sendbuf[2] = 0x02;
				sendbuf[3] = 0x1a;
				sendbuf[5] = 0x02;
				sendbuf[6] = 0xf1;
				sendbuf[7] = 0xaa;
				memcpy(uart_send, sendbuf, 8);
				TXEN = SEND;
				USART_SendDataString(8);
				comRecevieFlag = 2;
				sendCount = 0;
			}
			else if( sendCount > 2000)
				sendCount = 0;
			sendCount++;
		}
		#endif
		if (dealwithTag)
		{  
		  dealwithTag--;
		  if(dealwithTag == 1)//&& !Serial_Master )	
			dealwithData();
		}
		if(serial_receive_timeout_count>0)  
		{
				serial_receive_timeout_count -- ; 
				if(serial_receive_timeout_count == 0)
				{
					serial_restart();
				}
		}
//		ram_left = uxTaskGetStackHighWaterMark(NULL);
//		printf("R%u", ram_left);
		delay_ms(5) ;
		
	}	
}

#ifdef T3PT12
void vI2C_READ(void *pvParameters)
{
	static u8 wait_ad_stable = 0 ;
	u8		i;
	float Rc[4] = {1118284 ,966354,895032,676148};
	float Rc1[4] = {1118284 ,966354,895032,676148};
		modbus.cal_flag	= 0 ;
	PT12_IO_Init();

	for( ;; )
	{
		if(modbus.cal_flag == 0)
		{
			wait_ad_stable ++ ;
			if(wait_ad_stable >3)
			{
				if(init_celpoint()) 
				{
					modbus.cal_flag = 1;
					for(i=0; i<4; i++)
					{
						Rc[i] = (float)rs_data[i] ;
						Rc1[i] = (float)rs_data[i+4] ;
					}
					min2method(&linear_K.temp_C, &linear_B.temp_C, 4, Rc, Rs);
					min2method(&linear_K_1.temp_C, &linear_B_1.temp_C, 4, Rc1, Rs1);
				}
			}
		
		}
		else
		{
			if(read_rtd_data())
			{
				update_temperature();
				control_input();
			}
			Flash_Write_Mass();
		}
		delay_ms(2000) ;				//if pulse changes ,store the data
	}	

}

#endif



 #if (defined T322AI)||(defined T38AI8AO6DO) ||(defined T36CTA)
void vSTORE_EEPTask(void *pvParameters )
{
	uint8_t  loop ;
	for( ;; )
	{
		
		for(loop=0; loop<MAX_AI_CHANNEL; loop++)
		{
			if(data_change[loop] == 1)
			{
				AT24CXX_WriteOneByte(EEP_PLUSE0_HI_HI+4*loop, modbus.pulse[loop].quarter[0]);
				AT24CXX_WriteOneByte(EEP_PLUSE0_HI_LO+4*loop, modbus.pulse[loop].quarter[1]);
				AT24CXX_WriteOneByte(EEP_PLUSE0_LO_HI+4*loop, modbus.pulse[loop].quarter[2]);
				AT24CXX_WriteOneByte(EEP_PLUSE0_LO_LO+4*loop, modbus.pulse[loop].quarter[3]);
				data_change[loop] = 0 ;
			}
		}
		
		delay_ms(3000) ;				//if pulse changes ,store the data

	}
	
}
#endif
#ifndef T3PT12
void vINPUTSTask( void *pvParameters )
{
	static u16 record_count = 0 ;

	inputs_init();
	for( ;; )
	{
		portDISABLE_INTERRUPTS() ;
		inputs_scan();	
		record_count ++ ;
		if(record_count == 10)
		{
			record_count= 0 ;
			control_input();
			Flash_Write_Mass();

		}
		portDISABLE_INTERRUPTS() ;
		delay_ms(200);
	}	
}
#endif
#if (defined T38AI8AO6DO) || (defined T36CTA)
void vOUTPUTSTask( void *pvParameters )
{

	output_init();
	for( ;; )
	{
//		
		update_digit_output();		
		control_output();
		output_refresh();
		delay_ms(100);
	}	
}

void vKEYTask( void *pvParameters )
{
	
	KEY_IO_Init();
	for( ;; )
	{
		KEY_Status_Scan();
		delay_ms(100);
	}	
}

#endif


void Inital_Bacnet_Server(void)
{
//	u32 Instance = 0x0c;
//	Device_Init();
//	Device_Set_Object_Instance_Number(Instance);
#ifndef T36CTA		
 Device_Init();
 Device_Set_Object_Instance_Number(Instance);  
 address_init();
 bip_set_broadcast_addr(0xffffffff);

#if  READ_WRITE_PROPERTY   
#ifdef T322AI
  AIS = MAX_AIS;
  AVS = MAX_AVS;
#endif	
#if (defined T38AI8AO6DO) || (defined T36CTA)
  AIS = MAX_AIS;
  AOS = MAX_AO;
  BOS = MAX_DO;
  AVS = MAX_AVS;
#endif	
#endif	
#endif
}
void vMSTP_TASK(void *pvParameters )
{
	#ifndef T36CTA
	uint16_t pdu_len = 0; 
	BACNET_ADDRESS  src;
	Inital_Bacnet_Server();
	dlmstp_init(NULL);
	Recievebuf_Initialize(0);
	for (;;)
    {
		if(modbus.protocal == BAC_MSTP)
		{
					pdu_len = datalink_receive(&src, &PDUBuffer[0], sizeof(PDUBuffer), 0,	BAC_MSTP);
					if(pdu_len) 
						{
							npdu_handler(&src, &PDUBuffer[0], pdu_len, BAC_MSTP);	
						} 
						
		}
//			modbus.stack[1] = uxTaskGetStackHighWaterMark(NULL);
			delay_ms(5);
	}
	#endif
}

void vNETTask( void *pvParameters )
{
	//uip_ipaddr_t ipaddr;
	
	//u8 tcp_server_tsta = 0XFF;
	//u8 tcp_client_tsta = 0XFF;
	//printf("Temco ARM Demo\r\n");
	u8 count = 0 ;
	while(tapdev_init())	//��ʼ��ENC28J60����
	{								   
		delay_ms(50);
		printf("tapdev_init() failed ...\r\n");
	}
	
    for( ;; )
	{
		uip_polling();	//����uip�¼���������뵽�û������ѭ������ 
		
		if((IP_Change == 1)||(update == 1))
		{
			count++ ;
			if(count == 10)
			{
				count = 0 ;
				IP_Change = 0 ;	
//				//if(!tapdev_init()) printf("Init fail\n\r");				
//				while(tapdev_init())	//��ʼ��ENC28J60����
//				{								   
//				//	printf("ENC28J60 Init Error!\r\n");
//				delay_ms(50);
//				};	
				SoftReset();
			}
			
		}
		IWDG_ReloadCounter(); 
//		modbus.stack[0] = uxTaskGetStackHighWaterMark(NULL);
		delay_ms(2);
    }
}



//uip�¼�������
//���뽫�ú��������û���ѭ��,ѭ������.
void uip_polling(void)
{
	u8 i;
	static struct timer periodic_timer, arp_timer;
	static u8 timer_ok = 0;	 
	if(timer_ok == 0)		//����ʼ��һ��
	{
		timer_ok = 1;
		timer_set(&periodic_timer, CLOCK_SECOND / 2); 	//����1��0.5��Ķ�ʱ�� 
		timer_set(&arp_timer, CLOCK_SECOND * 10);	   	//����1��10��Ķ�ʱ�� 
	}
	
	uip_len = tapdev_read();							//�������豸��ȡһ��IP��,�õ����ݳ���.uip_len��uip.c�ж���
	if(uip_len > 0)							 			//������
	{   
//		printf("uip_len");
		//����IP���ݰ�(ֻ��У��ͨ����IP���Żᱻ����) 
		if(BUF->type == htons(UIP_ETHTYPE_IP))			//�Ƿ���IP��? 
		{
			uip_arp_ipin();								//ȥ����̫��ͷ�ṹ������ARP��
			uip_input();   								//IP������			
			//������ĺ���ִ�к������Ҫ�������ݣ���ȫ�ֱ��� uip_len > 0
			//��Ҫ���͵�������uip_buf, ������uip_len  (����2��ȫ�ֱ���)		    
			if(uip_len > 0)								//��Ҫ��Ӧ����
			{
				uip_arp_out();							//����̫��ͷ�ṹ������������ʱ����Ҫ����ARP����
				tapdev_send();							//�������ݵ���̫��
			}
		}
		else if (BUF->type == htons(UIP_ETHTYPE_ARP))	//����arp����,�Ƿ���ARP�����?
		{
			uip_arp_arpin();
			
 			//������ĺ���ִ�к������Ҫ�������ݣ���ȫ�ֱ���uip_len>0
			//��Ҫ���͵�������uip_buf, ������uip_len(����2��ȫ�ֱ���)
 			if(uip_len > 0)
				tapdev_send();							//��Ҫ��������,��ͨ��tapdev_send����	 
		}
	}
	else if(timer_expired(&periodic_timer))				//0.5�붨ʱ����ʱ
	{
		timer_reset(&periodic_timer);					//��λ0.5�붨ʱ�� 
		
		//��������ÿ��TCP����, UIP_CONNSȱʡ��40��  
		for(i = 0; i < UIP_CONNS; i++)
		{
			 uip_periodic(i);							//����TCPͨ���¼�
			
	 		//������ĺ���ִ�к������Ҫ�������ݣ���ȫ�ֱ���uip_len>0
			//��Ҫ���͵�������uip_buf, ������uip_len (����2��ȫ�ֱ���)
	 		if(uip_len > 0)
			{
				uip_arp_out();							//����̫��ͷ�ṹ������������ʱ����Ҫ����ARP����
				tapdev_send();							//�������ݵ���̫��
			}
		}
		
#if UIP_UDP	//UIP_UDP 
		//��������ÿ��UDP����, UIP_UDP_CONNSȱʡ��10��
		for(i = 0; i < UIP_UDP_CONNS; i++)
		{
			uip_udp_periodic(i);						//����UDPͨ���¼�
			
	 		//������ĺ���ִ�к������Ҫ�������ݣ���ȫ�ֱ���uip_len>0
			//��Ҫ���͵�������uip_buf, ������uip_len (����2��ȫ�ֱ���)
			if(uip_len > 0)
			{
				uip_arp_out();							//����̫��ͷ�ṹ������������ʱ����Ҫ����ARP����
				tapdev_send();							//�������ݵ���̫��
			}
		}
#endif 
		//ÿ��10�����1��ARP��ʱ������ ���ڶ���ARP����,ARP��10�����һ�Σ��ɵ���Ŀ�ᱻ����
		if(timer_expired(&arp_timer))
		{
			timer_reset(&arp_timer);
			uip_arp_timer();
		}
	}
}


void EEP_Dat_Init(void)
{
		u8 loop ;
		u8 temp[6]; 
		AT24CXX_Init();
//		panelname[0] = 'T' ;
//		panelname[1] = 'e' ;
//		panelname[2] = 's' ;
//		panelname[3] = 't' ;
	
//		AT24CXX_Read(EEP_PANEL_NAME1, panelname, 20); 
//		if((panelname[0] ==0xff)&&(panelname[1] ==0xff)&&(panelname[2] ==0xff)&&(panelname[3] ==0xff)&&(panelname[4] ==0xff))
		{			
			#ifdef T322AI	
			Set_Object_Name("T3_22AI");
			#endif
			#ifdef T38AI8AO6DO	
			Set_Object_Name("T3_8AO6DO");
			#endif	
			#ifdef T36CTA
			Set_Object_Name("T3_6CTA");
			#endif
			#ifdef 	T3PT12
			Set_Object_Name("T3_PT12");
			#endif
		}
		modbus.serial_Num[0] = AT24CXX_ReadOneByte(EEP_SERIALNUMBER_LOWORD);
		modbus.serial_Num[1] = AT24CXX_ReadOneByte(EEP_SERIALNUMBER_LOWORD+1);
		modbus.serial_Num[2] = AT24CXX_ReadOneByte(EEP_SERIALNUMBER_HIWORD);
		modbus.serial_Num[3] = AT24CXX_ReadOneByte(EEP_SERIALNUMBER_HIWORD+1);
	
		if((modbus.serial_Num[0]==0xff)&&(modbus.serial_Num[1]== 0xff)&&(modbus.serial_Num[2] == 0xff)&&(modbus.serial_Num[3] == 0xff))
		{
					modbus.serial_Num[0] = 1 ;
					modbus.serial_Num[1] = 1 ;
					modbus.serial_Num[2] = 2 ;
					modbus.serial_Num[3] = 2 ;
//					AT24CXX_WriteOneByte(EEP_SERIALNUMBER_LOWORD, modbus.serial_Num[0]);
//					AT24CXX_WriteOneByte(EEP_SERIALNUMBER_LOWORD+1, modbus.serial_Num[1]);
//					AT24CXX_WriteOneByte(EEP_SERIALNUMBER_LOWORD+2, modbus.serial_Num[2]);
//					AT24CXX_WriteOneByte(EEP_SERIALNUMBER_LOWORD+3, modbus.serial_Num[3]);
		}
		Instance = modbus.serial_Num[0] + (U16_T)(modbus.serial_Num[1] << 8);
		AT24CXX_WriteOneByte(EEP_VERSION_NUMBER_LO, SOFTREV&0XFF);
		AT24CXX_WriteOneByte(EEP_VERSION_NUMBER_HI, (SOFTREV>>8)&0XFF);
		modbus.address = AT24CXX_ReadOneByte(EEP_ADDRESS);
		if((modbus.address == 255)||(modbus.address == 0))
		{
					modbus.address = 254 ;
					
					AT24CXX_WriteOneByte(EEP_ADDRESS, modbus.address);
		}
		 
		modbus.product = PRODUCT_ID ;
		Station_NUM = AT24CXX_ReadOneByte(EEP_STATION_NUM);
		if(Station_NUM > 127)
		{
					Station_NUM = 3 ;
		}
		temp[0] = AT24CXX_ReadOneByte(EEP_BACNET_PORT_HI);
		temp[1] = AT24CXX_ReadOneByte(EEP_BACNET_PORT_LO);
		if(temp[0]== 0xff & temp[1] == 0xff)
		{
			modbus.bacnet_port = 47808 ; 
		}
		else 
		{
			modbus.bacnet_port = (temp[0]<<8)|temp[1] ;
		}
		modbus.hardware_Rev = AT24CXX_ReadOneByte(EEP_HARDWARE_REV);
		if((modbus.hardware_Rev == 255)||(modbus.hardware_Rev == 0))
		{
					modbus.hardware_Rev = HW_VER ;
		//			AT24CXX_WriteOneByte(EEP_HARDWARE_REV, modbus.hardware_Rev);
		}
		modbus.update = AT24CXX_ReadOneByte(EEP_UPDATE_STATUS);
		modbus.SNWriteflag = AT24CXX_ReadOneByte(EEP_SERIALNUMBER_WRITE_FLAG);
		
		modbus.baud = AT24CXX_ReadOneByte(EEP_BAUDRATE);
		if(modbus.baud > 5) 
		{	
			modbus.baud = 1 ;
//			AT24CXX_WriteOneByte(EEP_BAUDRATE, modbus.baud);
		}
		switch(modbus.baud)
				{
					case 0:
						modbus.baudrate = BAUDRATE_9600 ;
						SERIAL_RECEIVE_TIMEOUT = 6;
					break ;
					case 1:
						modbus.baudrate = BAUDRATE_19200 ;
						SERIAL_RECEIVE_TIMEOUT = 3;
					break;
					case 2:
						modbus.baudrate = BAUDRATE_38400 ;
						SERIAL_RECEIVE_TIMEOUT = 2;
					break;
					case 3:
						modbus.baudrate = BAUDRATE_57600 ;
						SERIAL_RECEIVE_TIMEOUT = 1;
					break;
					case 4:
						modbus.baudrate = BAUDRATE_115200 ;
						SERIAL_RECEIVE_TIMEOUT = 1;
					break;
					case 5:
						modbus.baudrate = BAUDRATE_76800 ;	
						SERIAL_RECEIVE_TIMEOUT = 1;
					break;
					default:
					break ;				
				}
				uart1_init(modbus.baudrate);

				
				#if T36CTA
				temp[0] = AT24CXX_ReadOneByte(EEP_RFM12_ENCRYPT_KEY1);
				if((temp[0]>=0x21)&&(temp[0]<=0x7f))
				{
					for(loop = 0; loop <16 ;loop++)
					{
						rfm69_key[loop] = AT24CXX_ReadOneByte(EEP_RFM12_ENCRYPT_KEY1+loop);
					}
				}
				RFM69_networkID = (AT24CXX_ReadOneByte(EEP_RFM69_NETWORK_ID_HI)<<8)|AT24CXX_ReadOneByte(EEP_RFM69_NETWORK_ID_LO);
				if((RFM69_networkID == 0xffff)||(RFM69_networkID == 0))
				{
					RFM69_networkID = 0x1;
				}
				
				RFM69_nodeID = AT24CXX_ReadOneByte(EEP_RFM69_NODE_ID);
				if((RFM69_nodeID == 0xff)|| (RFM69_nodeID == 0))
				{
					RFM69_nodeID = 10;
				}
				RFM69_freq = ((AT24CXX_ReadOneByte(EEP_RFM69_FREQ_1)<<24)|(AT24CXX_ReadOneByte(EEP_RFM69_FREQ_2)<<16)
								|(AT24CXX_ReadOneByte(EEP_RFM69_FREQ_3)<<8)|(AT24CXX_ReadOneByte(EEP_RFM69_FREQ_4)));
				if((RFM69_freq == 0xffffffff) || (RFM69_freq == 0))
				{
					RFM69_freq = 915000000;
				}
				rfm69_set_deadMaster = (AT24CXX_ReadOneByte(EEP_RFM69_DEADMASTER_HI)<<8)|(AT24CXX_ReadOneByte(EEP_RFM69_DEADMASTER_LO));
				rfm69_deadMaster = rfm69_set_deadMaster;
				if((rfm69_set_deadMaster == 0) || (rfm69_set_deadMaster == 0xffff))
				{
					rfm69_deadMaster = RFM69_DEFAULT_DEADMASTER;
					rfm69_set_deadMaster = RFM69_DEFAULT_DEADMASTER;
				}
				RFM69_biterate = (AT24CXX_ReadOneByte(EEP_RFM69_BITRATE_HI)<<8)|(AT24CXX_ReadOneByte(EEP_RFM69_BITRATE_LO));
				if((RFM69_biterate == 0) || (RFM69_biterate == 0xffff))
				{
					//RFM69_setBitRate(0x0d05);
					RFM69_biterate = 0x0d05;
				}
//				CT_first_AD = (AT24CXX_ReadOneByte(EEP_CT_FIRST_AD_HI)<<8)|AT24CXX_ReadOneByte(EEP_CT_FIRST_AD_LO);
//				if((CT_first_AD == 0xffff)||(CT_first_AD == 0))
//				{
//					
//					CT_first_AD = 2260;
//				}
//				CT_multiple = (AT24CXX_ReadOneByte(EEP_CT_MULTIPLE_HI)<<8)|AT24CXX_ReadOneByte(EEP_CT_MULTIPLE_LO);
//				if((CT_multiple== 0xffff)||(CT_multiple== 0))
//				{
//					CT_multiple = 174;
//				}
				acc_sensitivity[0] = (AT24CXX_ReadOneByte(EEP_ACC_SENSITIVITY_LO_HI)<<8)|AT24CXX_ReadOneByte(EEP_ACC_SENSITIVITY_LO_LO);
				if( (acc_sensitivity[0]== 0xffff)||(acc_sensitivity[0]== 0))
				{
					acc_sensitivity[0] = 50;
				}
				acc_sensitivity[1] = (AT24CXX_ReadOneByte(EEP_ACC_SENSITIVITY_HI_HI)<<8)|AT24CXX_ReadOneByte(EEP_ACC_SENSITIVITY_HI_LO);
				if( (acc_sensitivity[1]== 0xffff)||(acc_sensitivity[1]== 0))
				{
					acc_sensitivity[1] = 970;
				}
				comSwitch = AT24CXX_ReadOneByte(EEP_COM_SWITCH);
				
				masterPollCount  =(AT24CXX_ReadOneByte(EEP_COM_MASTER_POLL_TIME_HI)<<8 | AT24CXX_ReadOneByte(EEP_COM_MASTER_POLL_TIME_LO));
				if( (masterPollCount== 0xffff)||(masterPollCount == 0))
				{
					masterPollCount = 200;
				}
				
				#endif
				
				modbus.protocal = AT24CXX_ReadOneByte(EEP_MODBUS_COM_CONFIG); 
				if((modbus.protocal != MODBUS)&&(modbus.protocal != BAC_MSTP))
				{
					modbus.protocal = MODBUS;
				}
				for(loop = 0 ; loop<6; loop++)
				{
					temp[loop] = AT24CXX_ReadOneByte(EEP_MAC_ADDRESS_1+loop); 
				}
				if((temp[0]== 0xff)&&(temp[1]== 0xff)&&(temp[2]== 0xff)&&(temp[3]== 0xff)&&(temp[4]== 0xff)&&(temp[5]== 0xff) )
				{
					temp[0] = 0x04 ;
					temp[1] = 0x02 ;
					temp[2] = 0x35 ;
					temp[3] = 0xaF ;
					temp[4] = 0x00 ;
					temp[5] = 0x01 ;
//					AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_1, temp[0]);
//					AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_2, temp[1]);
//					AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_3, temp[2]);
//					AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_4, temp[3]);
//					AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_5, temp[4]);
//					AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_6, temp[5]);		
				}
				for(loop =0; loop<6; loop++)
				{
					modbus.mac_addr[loop] =  temp[loop]	;
				}
				
				for(loop = 0 ; loop<4; loop++)
				{
					temp[loop] = AT24CXX_ReadOneByte(EEP_IP_ADDRESS_1+loop); 
				}
				if((temp[0]== 0xff)&&(temp[1]== 0xff)&&(temp[2]== 0xff)&&(temp[3]== 0xff) )
				{
					temp[0] = 192 ;
					temp[1] = 168 ;
					temp[2] = 0 ;
					temp[3] = 3 ;
					AT24CXX_WriteOneByte(EEP_IP_ADDRESS_1, temp[0]);
					AT24CXX_WriteOneByte(EEP_IP_ADDRESS_2, temp[1]);
					AT24CXX_WriteOneByte(EEP_IP_ADDRESS_3, temp[2]);
					AT24CXX_WriteOneByte(EEP_IP_ADDRESS_4, temp[3]);
				}
				for(loop = 0 ; loop<4; loop++)
				{
					modbus.ip_addr[loop] = 	temp[loop] ;
					modbus.ghost_ip_addr[loop] = modbus.ip_addr[loop] ;
					//printf("%u,%u,%u,%u,%u,%u,%u,%u,",  modbus.ip_addr[0], modbus.ip_addr[1], modbus.ip_addr[2], modbus.ip_addr[3],temp[0],temp[1],temp[2],temp[3]);
				}
				
				temp[0] = AT24CXX_ReadOneByte(EEP_IP_MODE);
				if(temp[0] >1)
				{
					temp[0] = 0 ;
					AT24CXX_WriteOneByte(EEP_IP_MODE, temp[0]);	
				}
				modbus.ip_mode = temp[0] ;
				modbus.ghost_ip_mode = modbus.ip_mode ;
				
				
				for(loop = 0 ; loop<4; loop++)
				{
					temp[loop] = AT24CXX_ReadOneByte(EEP_SUB_MASK_ADDRESS_1+loop); 
				}
				if((temp[0]== 0xff)&&(temp[1]== 0xff)&&(temp[2]== 0xff)&&(temp[3]== 0xff) )
				{
					temp[0] = 0xff ;
					temp[1] = 0xff ;
					temp[2] = 0xff ;
					temp[3] = 0 ;
					AT24CXX_WriteOneByte(EEP_SUB_MASK_ADDRESS_1, temp[0]);
					AT24CXX_WriteOneByte(EEP_SUB_MASK_ADDRESS_2, temp[1]);
					AT24CXX_WriteOneByte(EEP_SUB_MASK_ADDRESS_3, temp[2]);
					AT24CXX_WriteOneByte(EEP_SUB_MASK_ADDRESS_4, temp[3]);
				
				}				
				for(loop = 0 ; loop<4; loop++)
				{
					modbus.mask_addr[loop] = 	temp[loop] ;
					modbus.ghost_mask_addr[loop] = modbus.mask_addr[loop] ;
				}
				
				for(loop = 0 ; loop<4; loop++)
				{
					temp[loop] = AT24CXX_ReadOneByte(EEP_GATEWAY_ADDRESS_1+loop); 
				}
				if((temp[0]== 0xff)&&(temp[1]== 0xff)&&(temp[2]== 0xff)&&(temp[3]== 0xff) )
				{
					temp[0] = 192 ;
					temp[1] = 168 ;
					temp[2] = 0 ;
					temp[3] = 4 ;
					AT24CXX_WriteOneByte(EEP_GATEWAY_ADDRESS_1, temp[0]);
					AT24CXX_WriteOneByte(EEP_GATEWAY_ADDRESS_2, temp[1]);
					AT24CXX_WriteOneByte(EEP_GATEWAY_ADDRESS_3, temp[2]);
					AT24CXX_WriteOneByte(EEP_GATEWAY_ADDRESS_4, temp[3]);
				
				}				
				for(loop = 0 ; loop<4; loop++)
				{
					modbus.gate_addr[loop] = 	temp[loop] ;
					modbus.ghost_gate_addr[loop] = modbus.gate_addr[loop] ;
				}
				
				temp[0] = AT24CXX_ReadOneByte(EEP_TCP_SERVER);
				if(temp[0] == 0xff)
				{
					temp[0] = 0 ;
					AT24CXX_WriteOneByte(EEP_TCP_SERVER, temp[0]);
				}
				modbus.tcp_server = temp[0];
				modbus.ghost_tcp_server = modbus.tcp_server  ;
				
				temp[0] =AT24CXX_ReadOneByte(EEP_LISTEN_PORT_HI);
				temp[1] =AT24CXX_ReadOneByte(EEP_LISTEN_PORT_LO);
				if(temp[0] == 0xff && temp[1] == 0xff )
				{
					modbus.listen_port = 502 ;
					temp[0] = (modbus.listen_port>>8)&0xff ;
					temp[1] = modbus.listen_port&0xff ;				
				}
				modbus.listen_port = (temp[0]<<8)|temp[1] ;
				modbus.ghost_listen_port = modbus.listen_port ;
				
				modbus.write_ghost_system = 0 ;
				modbus.reset = 0 ;
//		#ifndef T3PT12
//		for(loop = 0; loop < 11; loop++)
//		{
//			temp[0] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE1_VOL_HI_0+4*loop);
//			temp[1] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE1_VOL_HI_0+1+4*loop);
//			temp[2] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE1_VOL_HI_0+2+4*loop);
//			temp[3] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE1_VOL_HI_0+3+4*loop);
//			if((temp[0] == 0xff)&&(temp[1]==0xff)&&(temp[2]==0xff)&&(temp[3]==0xff))
//			{
//				temp[0] = 0 ;
//				temp[1]= 0 ;
//				temp[2] = 0 ;
//				temp[3] = 0 ;
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE1_VOL_HI_0+4*loop, temp[0]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE1_VOL_HI_0+4*loop+1, temp[1]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE1_VOL_HI_0+4*loop+2, temp[2]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE1_VOL_HI_0+4*loop+3, temp[3]);
//				modbus.customer_table1_val[loop] = 0 ;
//				modbus.customer_table1_vol[loop] = 0 ;
//			}
//			else
//			{
//				modbus.customer_table1_vol[loop] = (temp[0]<<8)+temp[1] ;
//				modbus.customer_table1_val[loop] = (temp[2]<<8)+temp[3] ;
//			}
//			
//			temp[0] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE2_VOL_HI_0+4*loop);
//			temp[1] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE2_VOL_HI_0+1+4*loop);
//			temp[2] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE2_VOL_HI_0+2+4*loop);
//			temp[3] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE2_VOL_HI_0+3+4*loop);
//			if((temp[0] == 0xff)&&(temp[1]==0xff)&&(temp[2]==0xff)&&(temp[3]==0xff))
//			{
//				temp[0] = 0 ;
//				temp[1]= 0 ;
//				temp[2] = 0 ;
//				temp[3] = 0 ;
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE2_VOL_HI_0+4*loop, temp[0]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE2_VOL_HI_0+4*loop+1, temp[1]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE2_VOL_HI_0+4*loop+2, temp[2]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE2_VOL_HI_0+4*loop+3, temp[3]);
//				modbus.customer_table2_val[loop] = 0 ;
//				modbus.customer_table2_vol[loop] = 0 ;
//			}
//			else
//			{
//				modbus.customer_table2_vol[loop] = (temp[0]<<8)+temp[1] ;
//				modbus.customer_table2_val[loop] = (temp[2]<<8)+temp[3] ;
//			}
//			
//			temp[0] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE3_VOL_HI_0+4*loop);
//			temp[1] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE3_VOL_HI_0+1+4*loop);
//			temp[2] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE3_VOL_HI_0+2+4*loop);
//			temp[3] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE3_VOL_HI_0+3+4*loop);
//			if((temp[0] == 0xff)&&(temp[1]==0xff)&&(temp[2]==0xff)&&(temp[3]==0xff))
//			{
//				temp[0] = 0 ;
//				temp[1]= 0 ;
//				temp[2] = 0 ;
//				temp[3] = 0 ;
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE3_VOL_HI_0+4*loop, temp[0]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE3_VOL_HI_0+4*loop+1, temp[1]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE3_VOL_HI_0+4*loop+2, temp[2]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE3_VOL_HI_0+4*loop+3, temp[3]);
//				modbus.customer_table3_val[loop] = 0 ;
//				modbus.customer_table3_vol[loop] = 0 ;
//			}
//			else
//			{
//				modbus.customer_table3_vol[loop] = (temp[0]<<8)+temp[1] ;
//				modbus.customer_table3_val[loop] = (temp[2]<<8)+temp[3] ;
//			}
//			
//			temp[0] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE4_VOL_HI_0+4*loop);
//			temp[1] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE4_VOL_HI_0+1+4*loop);
//			temp[2] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE4_VOL_HI_0+2+4*loop);
//			temp[3] =AT24CXX_ReadOneByte(EEP_CUSTOMER_TABLE4_VOL_HI_0+3+4*loop);
//			if((temp[0] == 0xff)&&(temp[1]==0xff)&&(temp[2]==0xff)&&(temp[3]==0xff))
//			{
//				temp[0] = 0 ;
//				temp[1]= 0 ;
//				temp[2] = 0 ;
//				temp[3] = 0 ;
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE4_VOL_HI_0+4*loop, temp[0]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE4_VOL_HI_0+4*loop+1, temp[1]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE4_VOL_HI_0+4*loop+2, temp[2]);
////				AT24CXX_WriteOneByte(EEP_CUSTOMER_TABLE4_VOL_HI_0+4*loop+3, temp[3]);
//				modbus.customer_table4_val[loop] = 0 ;
//				modbus.customer_table4_vol[loop] = 0 ;
//			}
//			else
//			{
//				modbus.customer_table4_vol[loop] = (temp[0]<<8)+temp[1] ;
//				modbus.customer_table4_val[loop] = (temp[2]<<8)+temp[3] ;
//			}
//			

//		}
//#endif		
		#ifdef T322AI
		for(loop = 0; loop < 22; loop++)
		{
				
				modbus.pulse[loop].quarter[0] = AT24CXX_ReadOneByte(EEP_PLUSE0_HI_HI+4*loop) ;
				modbus.pulse[loop].quarter[1] = AT24CXX_ReadOneByte(EEP_PLUSE0_HI_LO+4*loop);
				modbus.pulse[loop].quarter[2] = AT24CXX_ReadOneByte(EEP_PLUSE0_LO_HI+4*loop) ;
				modbus.pulse[loop].quarter[3] = AT24CXX_ReadOneByte(EEP_PLUSE0_LO_LO+4*loop);
				if((modbus.pulse[loop].quarter[0] == 0xff)&&(modbus.pulse[loop].quarter[1]== 0xff)&&(modbus.pulse[loop].quarter[2]== 0xff)&& (modbus.pulse[loop].quarter[3] == 0xff))
				{
					modbus.pulse[loop].word = 0 ;
//					AT24CXX_WriteOneByte(EEP_PLUSE0_HI_HI+4*loop, modbus.pulse[loop].quarter[0]);
//					AT24CXX_WriteOneByte(EEP_PLUSE0_HI_LO+4*loop, modbus.pulse[loop].quarter[1]);
//					AT24CXX_WriteOneByte(EEP_PLUSE0_LO_HI+4*loop, modbus.pulse[loop].quarter[2]);
//					AT24CXX_WriteOneByte(EEP_PLUSE0_LO_LO+4*loop, modbus.pulse[loop].quarter[3]);
				}
				
		
		}
		#endif
		#if (defined T38AI8AO6DO) || (defined T36CTA)
//		for(loop=0; loop<MAX_AO; loop++)
//		{				
//				temp[1]	= AT24CXX_ReadOneByte(EEP_AO_CHANNLE0+2*loop);
//				temp[2]	= AT24CXX_ReadOneByte(EEP_AO_CHANNLE0+1+2*loop);
//				if((temp[1]== 0xff)&&(temp[2] == 0xff))
//				{
//					outputs[loop].value = 0 ;
//					AT24CXX_WriteOneByte(EEP_AO_CHANNLE0+2*loop+1, 0);
//					AT24CXX_WriteOneByte(EEP_AO_CHANNLE0+2*loop, 0);
//				}
//				else
//				{
//						outputs[loop].value = (temp[2]<<8)| temp[1] ;		
//				}
//		}
//		for(loop=MAX_AO; loop<MAX_DO+MAX_AO; loop++)
//		{
//			outputs[loop].value = AT24CXX_ReadOneByte(EEP_DO_CHANNLE0+loop);
//			if(outputs[loop].value> 1) 
//			{
//					outputs[loop].value = 1 ;
//					AT24CXX_WriteOneByte(EEP_DO_CHANNLE0+loop, outputs[loop].value);
//			}
//		}
		for(loop=0; loop<MAX_AI_CHANNEL; loop++)
		{
				inputs[loop].filter = AT24CXX_ReadOneByte(EEP_AI_FILTER0+loop);
				if(inputs[loop].filter == 0xff)
				{
						inputs[loop].filter = DEFAULT_FILTER ;
//						AT24CXX_WriteOneByte(EEP_AI_FILTER0+loop, inputs[loop].filter);
				}
				inputs[loop].range = AT24CXX_ReadOneByte(EEP_AI_RANGE0+loop) ;
				if(inputs[loop].range == 0xff)
				{
						inputs[loop].range = 0 ;
//						AT24CXX_WriteOneByte(EEP_AI_RANGE0+loop, inputs[loop].range);
				}
				
				inputs[loop].calibration_hi = AT24CXX_ReadOneByte(EEP_AI_OFFSET0+2*loop) ;
				inputs[loop].calibration_lo = AT24CXX_ReadOneByte(EEP_AI_OFFSET0+2*loop +1) ;
				if((temp[1]== 0xff)&&(temp[2] == 0xff))
				{
						inputs[loop].calibration_hi = (500>>8)&0xff ;
						inputs[loop].calibration_lo = 500&0xff ;
						temp[1] = inputs[loop].calibration_hi ;
						temp[2] = inputs[loop].calibration_lo ;
//						AT24CXX_WriteOneByte(EEP_AI_OFFSET0+2*loop, temp[1]);
//						AT24CXX_WriteOneByte(EEP_AI_OFFSET0+2*loop+1, temp[2]);
				}

				modbus.pulse[loop].quarter[0] = AT24CXX_ReadOneByte(EEP_PLUSE0_HI_HI+4*loop) ;
				modbus.pulse[loop].quarter[1] = AT24CXX_ReadOneByte(EEP_PLUSE0_HI_LO+4*loop);
				modbus.pulse[loop].quarter[2] = AT24CXX_ReadOneByte(EEP_PLUSE0_LO_HI+4*loop) ;
				modbus.pulse[loop].quarter[3] = AT24CXX_ReadOneByte(EEP_PLUSE0_LO_LO+4*loop);
				if((modbus.pulse[loop].quarter[0] == 0xff)&&(modbus.pulse[loop].quarter[1]== 0xff)&&(modbus.pulse[loop].quarter[2]== 0xff)&& (modbus.pulse[loop].quarter[3] == 0xff))
				{
					modbus.pulse[loop].word = 0 ;
					AT24CXX_WriteOneByte(EEP_PLUSE0_HI_HI+4*loop, modbus.pulse[loop].quarter[0]);
					AT24CXX_WriteOneByte(EEP_PLUSE0_HI_LO+4*loop, modbus.pulse[loop].quarter[1]);
					AT24CXX_WriteOneByte(EEP_PLUSE0_LO_HI+4*loop, modbus.pulse[loop].quarter[2]);
					AT24CXX_WriteOneByte(EEP_PLUSE0_LO_LO+4*loop, modbus.pulse[loop].quarter[3]);
				}
				
		}
		#endif
	
}

//u16 swap_int16( u16 value)
//{
//	u8 temp1, temp2 ;
//	temp1 = value &0xff ;
//	temp2 = (value>>8)&0xff ;
//	
//	return  (temp1<<8)|temp2 ;
//}

//u32 swap_int32( u32 value)
//{
//	u8 temp1, temp2, temp3, temp4 ;
//	temp1 = value &0xff ;
//	temp2 = (value>>8)&0xff ;
//	temp3 = (value>>16)&0xff ;
//	temp4 = (value>>24)&0xff ;
//	
//	return  ((u32)temp1<<24)|((u32)temp2<<16)|((u32)temp3<<8)|temp4 ;
//}
