/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "math.h"
#include<string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_RESOLITON 4095 //  basınç sensörü için max değer (2^12-1)=4095 -- 12-bit ADC max değeri
#define ADC_VOLTAGE_RANGE 5.0f //proteus simülasyonundaki ADC referans voltajı -- basınç sensörü için - 5Volt
#define PRESSURE_MIN 0.0 // Minimum basınç - 0 kPa
#define VOLTAGE_MIN 0.26 //Sensörün 0 kPa'daki çıkış voltajı - 0,26V
#define PRESSURE_MAX 250.0f //Maksimum basınç kPa - 250 kPa
#define VOLTAGE_MAX 4.87  //Sensörün 250 kPa'daki çıkış voltajı - 4,87V
#define OVER_PRESSURE_LIMIT 200.0f // Kritik Aşırı Basınç Eşiği (200 kPa)

/*Message String*/
#define MSG_EMERGENCY "EMERGENCY STOP ACTIVATED \n\r"	//kesme aktifleştiğinde seri monitörde görülecek mesaj
#define MSG_OVER_PRESSURE "WARNING: OVER PRESSURE LIMIT EXCEEDED! \n\r"	//kritik aşırı basınç eşiğine ulaşıldığında seri monitörde görülecek mesaj

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
char uartMsg[50]; //UART mesaj bufferı
volatile uint32_t pwm_pulse = 0; // Motor hızını belirleyen PWM karşılaştırma değerini tutacak
volatile uint8_t emergency_flag = 0; // Acil stop durum bayrağı
volatile uint8_t timer_flag = 0; // TIM3 her 50 ms'de bu bayrağı set edecek
float water_level_cm = 0.0f; // Hesaplanan su seviyesi (cm)
volatile uint8_t over_pressure_msg_sent = 0; // Aşım olduğunda mesajın 1 kez atılmasını sağlayacak
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Buton Dış Kesmesi (Acil Stop)
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{

	    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET)	//eğer butona basıldıysa -- devre ters mantık çalıştığından ötürü set değil reset olarak kodlandı
	    {
	      emergency_flag = 1; // Acil durum aktif

	      HAL_GPIO_WritePin(LED_UYARI_GPIO_Port, LED_UYARI_Pin, GPIO_PIN_SET);	//uyarı ledi yandı

	      // 1. PWM sinyalini donanımsal olarak anında kes, yani motoru durdur-- devre ters çalıştığından ötürü 0 değil max değer olan 65535 değeri verildi
	      pwm_pulse = 65535;
	      __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 65535);

	      HAL_UART_Transmit(&huart1, (uint8_t*)MSG_EMERGENCY, strlen(MSG_EMERGENCY), 100);	//mesaj gönderildi
	    }
	    else // Buton bırakıldıysa
	    {
	      emergency_flag = 0; // Normal moda geç
	      HAL_GPIO_WritePin(LED_UYARI_GPIO_Port, LED_UYARI_Pin, GPIO_PIN_RESET);	//ledi söndür

	    }
}

//TIM3 her 50 ms'de bir bayrağı set eder
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM3)
	{
	  timer_flag = 1;
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

	  float sensitivity = (PRESSURE_MAX-PRESSURE_MIN)/(VOLTAGE_MAX-VOLTAGE_MIN);	//duyarlılık: 1 Volt değişim başına düşen basınç miktarı hesaplanıyor kPa/V
	  float offset      = VOLTAGE_MIN -(sensitivity*VOLTAGE_MIN);	//offset: Vmin kadar kayma düzeltir
	  float resoluyion  = ADC_VOLTAGE_RANGE/ADC_RESOLITON;	//çözünürlük: 1 ADC adımı başına düşen voltaj miktarını hesaplıyoruz 5V/4095
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  // Motor Yön Pinini Hazırla
    HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);

    // Sıçramayı önlemek için İLK AÇILIŞ SIFIRLAMASI & İLK ADC OKUMASI
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
      int first_adc = HAL_ADC_GetValue(&hadc1);

      float pressure_KPa = (first_adc * resoluyion * sensitivity) + offset;
      if (pressure_KPa < 0.0f) pressure_KPa = 0.0f;
      if (pressure_KPa > PRESSURE_MAX) pressure_KPa = PRESSURE_MAX;


      // Su seviyesi hesabı: P = rho * g * h  -->  h = P / 9.81
      water_level_cm = (pressure_KPa / 9.81f) * 100.0f;

      // İlk doğru PWM değerini hesapla
      pwm_pulse = (uint32_t)((pressure_KPa / PRESSURE_MAX) * 65535.0f);
    }
    HAL_ADC_Stop(&hadc1);

    //Hesaplanan İLK doğru değeri Timer Register'ına ÖNDEN yükle
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pwm_pulse);

    // PWM ve TIM3 Kesmesini ŞİMDİ Başlat (Sıçrama tamamen engellenecek)
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

    // TIM3 kesmesini aç ve 50 ms'lik zamanlayıcıyı başlat
     HAL_NVIC_SetPriority(TIM3_IRQn, 1, 0); // EXTI (0) her zaman daha öncelikli
     HAL_NVIC_EnableIRQ(TIM3_IRQn);
     HAL_TIM_Base_Start_IT(&htim3);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  //Sadece TIM3'ten 50 ms geldiğinde çalış
	  	  if (timer_flag == 1)
	  	  {
	  	    timer_flag = 0;

	  	    // Sadece Acil Durum YOKSA normal okuma ve motor sürme işlemini yap
	  	      if (emergency_flag == 0)
	  	      {
	  	        HAL_ADC_Start(&hadc1);

	  	        if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
	  	        {
	  	          int adc_value = HAL_ADC_GetValue(&hadc1);

	  	          float pressure_KPa = (adc_value * resoluyion * sensitivity) + offset;
	  	          if (pressure_KPa < 0.0f) pressure_KPa = 0.0f;
	  	          if (pressure_KPa > PRESSURE_MAX) pressure_KPa = PRESSURE_MAX;

	  	         water_level_cm = (pressure_KPa / 9.81f) * 100.0f;

	  	       // Kritik Aşırı Basınç Kontrolü (200 kPa Eşiği)
	  	       if (pressure_KPa >= OVER_PRESSURE_LIMIT)
	  	       {
	  	         // kritik aşırı basınç değerinin üzerindeysek motoru durdur ve LED'i yak
	  	         pwm_pulse = 65535;
	  	         __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 65535);
	  	         HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);

	  	         // Eğer mesaj daha önce gönderilmediyse SADECE 1 KEZ gönder
	  	         if (over_pressure_msg_sent == 0)
	  	         {
	  	           HAL_UART_Transmit(&huart1, (uint8_t*)MSG_OVER_PRESSURE, strlen(MSG_OVER_PRESSURE), 100);
	  	           over_pressure_msg_sent = 1; // Bayrağı kaldır, tekrar mesaj atılmasını engelle
	  	         }
	  	       }
	  	       else
	  	       {
	  	         // Normal Basınç: Anlık PWM değerini hesapla ve motora yükle
	  	         pwm_pulse = (uint32_t)((pressure_KPa / PRESSURE_MAX) * 65535.0f);
	  	         __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pwm_pulse);
	  	         HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);

	  	         // Basınç normale döndüğü için bayrağı sıfırla (Bir sonraki aşımda tekrar mesaj atabilsin)
	  	         over_pressure_msg_sent = 0;
	  	       }
	  	      }
	  	         HAL_ADC_Stop(&hadc1);
	  	     }
	  	  }

	  	  __WFI(); // kesme gelene kadar işlemci uyur

	    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */
  __HAL_RCC_ADC1_CLK_ENABLE();
  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 7999;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 49;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */

  /* GPIO Port Saatlerini Aktifleştir */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Çıkış Pin Seviyelerini Sıfırla (PB2: DIR, PB15: LED) */
   HAL_GPIO_WritePin(GPIOB, DIR_Pin | GPIO_PIN_15, GPIO_PIN_RESET);

  /* PA2 Pinini Analog Giriş Olarak Yapılandır (Basınç Sensörü) */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB2 & PB15 Pinlerini Çıkış Yap (PB2: DIR, PB15: Uyarı LED'i) */
      GPIO_InitStruct.Pin = DIR_Pin | GPIO_PIN_15;
      GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
      GPIO_InitStruct.Pull = GPIO_NOPULL;
      GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
      HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PA5: Acil Stop Butonu (EXTI Kesmesi - Pull-Up Modu) */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* EXTI Line 5..9 Kesmesini Aktif Et ve Öncelik Ver */
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

/* USER CODE END MX_GPIO_Init_2 */
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
  __disable_irq();
  while (1)
  {
  }
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
