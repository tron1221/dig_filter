/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2019 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

	volatile uint16_t dac_out=0;///<Последнее выданное на DAC значение
	volatile char usart_data[USART_DATA_AMOUNT]={'0','1','2','3',' ','m','V','\r'};///<Массив данных для отправки по USART
	
	uint16_t adc_buf0[ADC_DATA_AMOUNT];///<Буфер 0 полученных данных от ADC
	uint16_t adc_buf1[ADC_DATA_AMOUNT];///<Буфер 1 полученных данных от ADC
	
	uint16_t dac_buf0[DAC_DATA_AMOUNT];///<Буфер 0 данных отправленных на DAC
	uint16_t dac_buf1[DAC_DATA_AMOUNT];///<Буфер 1 данных отправленных на DAC
	
	void (* p_ADC_func_proc)(void)=NOP;///<Указатель на текущую функцию-обработчик массива данных от ADC
	void (* p_DAC_func_proc)(void)=NOP;///<Указатель на текущую функцию-обработчик массива данных для DAC
	
	uint16_t *p_adc_buf=adc_buf0;///<Указатель на массив данных от ADC нуждающийся в обработке
	
	volatile uint32_t dac_cur_buf_count=0;///<Счетчик текущего положения заполнения массива данных для DAC
	uint16_t *p_dac_cur_buf=dac_buf0;///<Указатель на текущий заполняемый массив данных для DAC
	uint16_t *p_dac_buf=dac_buf1;///<Указатель на массив данных для DAC нуждающийся в обработке
	
	///Набор возможных значений, отражающих текущий заполняемый массив
	enum dac_cur_buf_Type 
	{
	DAC_BUF0, ///<Указывает, что заполняется буфер данных 0
	DAC_BUF1 ///<Указывает, что заполняется буфер данных 1
	} dac_cur_buf=DAC_BUF0;///<Определяет текущий заполняемый буфер данных для DAC

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
@brief Запускает отправку массива данных по USART
@retval None
*/
void  usart_data_transmite(void)
{
	LED_ON(RED_LED);
	LL_DMA_DisableStream(DMA1,LL_DMA_STREAM_3);
	LL_DMA_SetDataLength(DMA1,LL_DMA_STREAM_3,USART_DATA_AMOUNT);
	LL_DMA_ClearFlag_TC3(DMA1);
	LL_DMA_ClearFlag_HT3(DMA1);
	LL_DMA_ClearFlag_TE3(DMA1);
	LL_DMA_EnableStream(DMA1,LL_DMA_STREAM_3);
	LL_USART_ClearFlag_TC(USART3);	
	LL_USART_EnableIT_TC(USART3);
	LL_USART_EnableDirectionTx(USART3);
}


/**
@brief Расчитывает среднее значение элементов массива
@param ar Массив данных для расчета среднего значения
@param ar_num Количество элементов массива
@retval uint16_t Среднее значение элементов массива. Если количество элементов массива меньше единицы, вернет первый элемент массива
*/
uint16_t calc_ar_average(uint16_t ar[],uint16_t ar_num)
{
	uint32_t sum=0;
	ar_num=(ar_num<=0)?1:ar_num;
	for(uint16_t i=0;i<ar_num;i++) sum+=ar[i];
	return sum/ar_num;
}

/**
@brief Обрабатывает массив данных, полученных от ADC
@retval None
*/
void ADC_data_proc(void)
{
	dac_out=*(p_dac_cur_buf+dac_cur_buf_count++)=calc_ar_average(p_adc_buf,ADC_DATA_AMOUNT);
	
	LL_DAC_ConvertData12RightAligned(DAC,LL_DAC_CHANNEL_1,dac_out);
	LED_TOGGLE(GREEN_LED);
	
	if(dac_cur_buf_count==DAC_DATA_AMOUNT) 
	{
		dac_cur_buf_count=0;
		p_DAC_func_proc=DAC_data_proc;
		if (dac_cur_buf==DAC_BUF0)
		{
			p_dac_cur_buf=dac_buf1;
			p_dac_buf=dac_buf0;
			dac_cur_buf=DAC_BUF1;
		}
		else
		{
			p_dac_cur_buf=dac_buf0;
			p_dac_buf=dac_buf1;
			dac_cur_buf=DAC_BUF0;
		}
	}
	p_ADC_func_proc=NOP;
}

/**
@brief Обрабатывает массив данных отправленных на DAC
@retval None
*/
void DAC_data_proc(void)
{
	uint32_t usart_num=calc_ar_average(p_dac_buf,DAC_DATA_AMOUNT); 
	char str[4]={'0','0','0','0'};
	usart_num*=3000;
	usart_num/=4095;
	sprintf(str,"%04u",(uint16_t)usart_num);
	usart_data[0]=str[0];
	usart_data[1]=str[1];
	usart_data[2]=str[2];
	usart_data[3]=str[3];
	
	usart_data_transmite();
	
	p_DAC_func_proc=NOP;
}
/**
@brief Функция-заглушка, не выполняет никаких действий
@retval None
*/
void NOP(void){}

/**
@brief Запускает преобразование DAC
@retval None
*/
static inline void DAC_start(void)
{
	//настраиваем DAC
	LL_DAC_DisableTrigger(DAC,LL_DAC_CHANNEL_1);
	LL_DAC_Enable(DAC,LL_DAC_CHANNEL_1);	
}

/**
@brief Подготавливает USART для передачи данных
@retval None
*/
static inline void USART_prep(void)
{
	//настройка DMA1, USART3 для передачи по USART	
	LL_USART_EnableDMAReq_TX(USART3);
	LL_DMA_SetPeriphAddress(DMA1,LL_DMA_STREAM_3,(uint32_t)&(USART3->DR));
	LL_DMA_SetMemoryAddress(DMA1,LL_DMA_STREAM_3,(uint32_t)usart_data);	
}
/**
@brief Запускает преобразование ADC
@retval None
*/
static inline void ADC_start(void)
{
	//настраиваем ADC
	LL_DMA_SetPeriphAddress(DMA2,LL_DMA_STREAM_0,(uint32_t)&(ADC1->DR));
	LL_DMA_SetMemoryAddress(DMA2,LL_DMA_STREAM_0,(uint32_t)adc_buf0);	
	LL_DMA_SetMemory1Address(DMA2,LL_DMA_STREAM_0,(uint32_t)adc_buf1);
	
	LL_DMA_SetDataLength(DMA2,LL_DMA_CHANNEL_0,ADC_DATA_AMOUNT);
	
	LL_DMA_EnableDoubleBufferMode(DMA2,LL_DMA_STREAM_0);
	
	LL_DMA_ClearFlag_TC0(DMA2);
	LL_DMA_ClearFlag_HT0(DMA2);
	LL_DMA_ClearFlag_TE0(DMA2);
	
	LL_DMA_SetCurrentTargetMem(DMA2,LL_DMA_STREAM_0,LL_DMA_CURRENTTARGETMEM0);
	LL_DMA_EnableIT_TC(DMA2,LL_DMA_STREAM_0);
	LL_DMA_EnableStream(DMA2,LL_DMA_STREAM_0);

	LL_ADC_Enable(ADC1);

	LL_TIM_EnableCounter(TIM3);
	LL_TIM_EnableIT_UPDATE(TIM3);	
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  

  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_1);

  /* System interrupt init*/

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_DAC_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
	
	//настраиваем DAC
	DAC_start();

	//настройка DMA1, USART3 для передачи по USART	
	USART_prep();
	
	//настраиваем ADC
	ADC_start();
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

while (1)
  {

		(* p_ADC_func_proc)();
		(* p_DAC_func_proc)();
		/* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_5);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_5)
  {
  Error_Handler();  
  }
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  LL_RCC_HSE_Enable();

   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {
    
  }
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_8, 336, LL_RCC_PLLP_DIV_2);
  LL_RCC_PLL_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL_IsReady() != 1)
  {
    
  }
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  
  }
  LL_Init1msTick(168000000);
  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
  LL_SetSystemCoreClock(168000000);
}

/* USER CODE BEGIN 4 */

 

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
