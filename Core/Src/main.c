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
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes --------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "w25n01.h"
#include "lfs.h"
#include "lfs_port.h"
#include "gps_logger.h"
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

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart5;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
#define RX_BUFFER_SIZE 300

uint8_t rx_char;
char rx_buffer[RX_BUFFER_SIZE];
uint16_t rx_index = 0;

uint8_t uart_rx_char;
uint8_t cmd_char;
uint8_t cmd_received = 0;
typedef enum
{
    LOGGER_MODE = 0,
    MENU_MODE
} SystemMode_t;

SystemMode_t systemMode = LOGGER_MODE;



#define LIS2DH12_ADDR     (0x19 << 1)

#define CTRL_REG1         0x20
#define OUT_X_L           0x28

uint32_t last_output_time = 0;

char gps_date[16] = "NA";
char gps_time_ist[16] = "NA";

double latitude_dd = 0.0;
double longitude_dd = 0.0;

uint8_t waitingFileNumber = 0;
uint8_t waitingDeleteConfirm = 0;
uint8_t selectedFileIndex = 0;
uint8_t currentMenu = 0;

uint8_t flash_id[3];
//uint16_t flashPage = 64;

//uint8_t firstWrite = 1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_UART5_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
void LIS2DH12_Init(void);
void LIS2DH12_Read(int16_t *x, int16_t *y, int16_t *z);

double NMEA_To_Decimal(char *coord);
void Parse_GNRMC(char *sentence);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart5, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

void PrintMenu(void)
{
    printf("\r\n");
    printf("=================================\r\n");
    printf(" GPS LOGGER MENU\r\n");
    printf("=================================\r\n");
    printf("1. List Files\r\n");
    printf("2. Read File\r\n");
    printf("3. Download File\r\n");
    printf("4. File Size\r\n");
    printf("5. Delete File\r\n");
    printf("6. Exit Menu\r\n");
    printf("=================================\r\n");
    printf("GPS> \r\n");
}

void PrintPrompt(void)
{
	printf("GPS> \r\n");
}



void ExecuteCommand(char cmd)
{
    if(systemMode == LOGGER_MODE)
    {
    	if(cmd == 'M' || cmd == 'm')
    	{
    	    currentMenu = 0;          // Reset submenu
    	    waitingFileNumber = 0;
    	    waitingDeleteConfirm = 0;

    	    systemMode = MENU_MODE;

    	    PrintMenu();
    	}
        else
        {
            printf("Press M for Menu\r\n");
        }

        return;
    }

    if(systemMode == MENU_MODE)
    {
        switch(cmd)
        {

        case '1':
            printf("\r\n");
            GPS_ListFiles();
            PrintPrompt();
            break;

        case '2':

            GPS_ScanFiles();

            if(fileCount == 0)
            {
                printf("\r\nNo Log Files Found\r\n");
                PrintPrompt();
                break;
            }

            currentMenu = 2;

            printf("\r\n");
            printf("========== READ FILE ==========\r\n");
            printf("Select File Number (1-%d)\r\n", fileCount);
            printf("0. Back\r\n");
            printf("Choice : ");

            break;


        case '3':      // Download File
        {
            GPS_ScanFiles();

            if(fileCount == 0)
            {
                printf("\r\nNo Log Files Found\r\n");
                PrintPrompt();
                break;
            }

            currentMenu = 5;

            printf("\r\n");
            printf("\r\n========== DOWNLOAD FILE ==========\r\n");

            GPS_ListFiles();

            printf("\r\nSelect File Number (1-%d)\r\n", fileCount);
            printf("0. Back\r\n");
            printf("Choice : ");
            break;
        }


        case '4':  //File size

            GPS_ScanFiles();

            if(fileCount == 0)
            {
                printf("\r\nNo Log Files Found\r\n");
                PrintPrompt();
                break;
            }

            currentMenu = 3;

            printf("\r\n");
            printf("========== FILE SIZE ==========\r\n");
            printf("Select File Number (1-%d)\r\n", fileCount);
            printf("0. Back\r\n");
            printf("Choice : ");

            break;






        case '5':   //Delete file

            GPS_ScanFiles();

            if(fileCount == 0)
            {
                printf("\r\nNo Log Files Found\r\n");
                PrintPrompt();
                break;
            }

            currentMenu = 4;

            printf("\r\n");
            printf("========== DELETE FILE ==========\r\n");
            printf("Select File Number (1-%d)\r\n", fileCount);
            printf("0. Back\r\n");
            printf("Choice : ");

            break;



        case '6':  //exit
        {
            systemMode = LOGGER_MODE;
            currentMenu = 0;

            printf("\r\nExit Menu\r\n");

            break;
        }

        }
    }
}

void ProcessSelectedFile(uint8_t index)
{
    switch(waitingFileNumber)
    {
        case 2:
            GPS_ReadLog(fileList[index]);
            break;

        case 3:
            GPS_GetFileSize(fileList[index]);
            break;

        case 4:
            selectedFileIndex = index;
            waitingDeleteConfirm = 1;

            printf("\r\nDelete \"%s\" ? (Y/N): ",
                   fileList[selectedFileIndex]);
            break;
    }

    waitingFileNumber = 0;

    if(!waitingDeleteConfirm)
    {
        PrintPrompt();
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
  MX_UART5_Init();
  MX_USART3_UART_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  printf("\r\nBOOT START\r\n");

  W25N01_Init();
  W25N01_Reset();

  printf("FLASH INIT DONE\r\n");

  W25N01_SetFeature(0xA0,0x00);

  HAL_Delay(5);

  HAL_UART_Receive_IT(&huart3, &rx_char, 1);
  HAL_UART_Receive_IT(&huart5, &uart_rx_char, 1);
  HAL_Delay(100);

  LIS2DH12_Init();

  HAL_Delay(100);

  printf("\r\n=================================\r\n");
  printf(" GPS LOGGER STARTED\r\n");
  printf("=================================\r\n");



  /* Initialize LittleFS */
  littlefs_init();

  /* Mount filesystem */
  int err = lfs_mount(&lfs, &cfg);

  if (err)
  {
      printf("\r\n=================================\r\n");
      printf(" LittleFS Mount Failed!\r\n");
      printf(" Error Code : %d\r\n", err);
      printf(" Existing Data Preserved\r\n");
      printf("=================================\r\n");

      /* Do NOT format the flash */
  }
  else
  {
      printf("LittleFS Mounted Successfully\r\n");
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (cmd_received)
	  {
	      cmd_received = 0;
	      if(waitingDeleteConfirm)
	      {
	          if(cmd_char == '\r' || cmd_char == '\n')
	          {
	              continue;
	          }

	          if(cmd_char == 'Y' || cmd_char == 'y')
	          {
	              waitingDeleteConfirm = 0;

	              printf("\r\nDeleting...\r\n\r\n");

	              if(GPS_DeleteLog(fileList[selectedFileIndex]))
	              {
	                  GPS_ScanFiles();
	              }
	          }
	          else if(cmd_char == 'N' || cmd_char == 'n')
	          {
	              waitingDeleteConfirm = 0;

	              printf("\r\nDelete Cancelled\r\n");
	          }
	          else
	          {
	              printf("\r\nInvalid Choice\r\n");
	              continue;
	          }

	          GPS_ScanFiles();

	          if(fileCount == 0)
	          {
	              currentMenu = 0;
	              printf("\r\nNo Log Files Found\r\n");
	              PrintPrompt();
	          }
	          else
	          {
	              printf("\r\n");
	              printf("========== DELETE FILE ==========\r\n");
	              printf("Select File Number (1-%d)\r\n", fileCount);
	              printf("0. Back\r\n");
	              printf("Choice : ");
	          }

	          continue;
	      }


	      if(currentMenu == 2)
	      {
	          if(cmd_char == '0')
	          {
	              currentMenu = 0;
	              PrintPrompt();
	          }
	          else
	          {
	              uint8_t index = cmd_char - '1';



	              if(index < fileCount)
	              {
	                  GPS_ReadLog(fileList[index]);
	              }
	              else
	              {
	                  printf("\r\nInvalid File Number\r\n");
	              }

	              printf("\r\n");
	              printf("Select File Number (1-%d)\r\n", fileCount);
	              printf("0. Back\r\n");
	              printf("Choice : ");
	          }

	          continue;
	      }



	      if(currentMenu == 3)
	      {
	          if(cmd_char == '0')
	          {
	              currentMenu = 0;
	              PrintPrompt();
	              continue;
	          }

	          uint8_t index = cmd_char - '1';


	          if(index < fileCount)
	          {
	              GPS_GetFileSize(fileList[index]);
	          }
	          else
	          {
	              printf("\r\nInvalid File Number\r\n");
	          }

	          printf("\r\n");
	          printf("Select File Number (1-%d)\r\n", fileCount);
	          printf("0. Back\r\n");
	          printf("Choice : ");

	          continue;
	      }


	      if(currentMenu == 4)
	      {
	    	  if(cmd_char == 0)
	    	      {
	    	          continue;
	    	      }

	          if(cmd_char == '0')
	          {
	              currentMenu = 0;
	              PrintPrompt();
	              continue;
	          }

	          uint8_t index = cmd_char - '1';


	          if(index < fileCount)
	          {
	              selectedFileIndex = index;

	              waitingDeleteConfirm = 1;

	              printf("\r\nDelete \"%s\" ? (Y/N): ",
	                     fileList[selectedFileIndex]);

	              fflush(stdout);

	              continue;
	          }
	          else
	          {
	              printf("\r\nInvalid File Number\r\n");

	              printf("\r\n");
	              printf("Select File Number (1-%d)\r\n", fileCount);
	              printf("0. Back\r\n");
	              printf("Choice : ");
	          }
	          cmd_received = 0;
	          continue;
	      }



	      if(currentMenu == 5)
	      {
	          if(cmd_char == '0')
	          {
	              currentMenu = 0;
	              PrintPrompt();
	              continue;
	          }

	          uint8_t index = cmd_char - '1';

	          if(index < fileCount)
	          {
	              GPS_DownloadLog(fileList[index]);

	              currentMenu = 0;
	              PrintPrompt();
	          }
	          else
	          {
	              printf("\r\nInvalid File Number\r\n");

	              printf("\r\n");
	              printf("========== DOWNLOAD FILE ==========\r\n");

	              GPS_ListFiles();

	              printf("\r\nSelect File Number (1-%d)\r\n", fileCount);
	              printf("0. Back\r\n");
	              printf("Choice : ");
	          }

	          continue;
	      }



	      else if(waitingFileNumber != 0)
	      {
	          uint8_t index = cmd_char - '1';

	          if(index < fileCount)
	          {
	              ProcessSelectedFile(index);
	          }
	          else
	          {
	              printf("\r\nInvalid File Number\r\n");
	          }
	      }
	      else
	      {
	          ExecuteCommand(cmd_char);
	          cmd_char = 0;
	      }
	  }

	  if(systemMode == LOGGER_MODE &&
	     HAL_GetTick() - last_output_time >= 5000)


	    {
	    	//flashPage = 64;
	        last_output_time = HAL_GetTick();

	        int16_t x, y, z;

	        LIS2DH12_Read(&x, &y, &z);

	        char logBuffer[128];
	        sprintf(logBuffer,
	                "DATE=%s TIME=%s LAT=%.6f LON=%.6f X=%d Y=%d Z=%d",
	                gps_date,
	                gps_time_ist,
	                latitude_dd,
	                longitude_dd,
	                x,
	                y,
	                z);

	        if (strcmp(gps_date, "NA") != 0)
	        {
	            printf("%s\r\n", logBuffer);
	            GPS_Log(logBuffer);
	        }
	        else
	        {
	            printf("Waiting for GPS date...\r\n");
	        }
	    }


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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_0;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV4;
  RCC_OscInitStruct.PLL.PLLM = 3;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 1;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }

}
/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x30909DEC;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  SPI_AutonomousModeConfTypeDef HAL_SPI_AutonomousMode_Cfg_Struct = {0};

  /* USER CODE BEGIN SPI1_Init 1 */
  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x7;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  hspi1.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
  hspi1.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerState = SPI_AUTO_MODE_DISABLE;
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerSelection = SPI_GRP1_GPDMA_CH0_TCF_TRG;
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerPolarity = SPI_TRIG_POLARITY_RISING;
  if (HAL_SPIEx_SetConfigAutonomousMode(&hspi1, &HAL_SPI_AutonomousMode_Cfg_Struct) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart5.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart5, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart5, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 38400;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

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

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA,
                    SPI_HOLD_Pin|SPI_CS_Pin|SPI_WP_Pin,
                    GPIO_PIN_SET);
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET);

  /*Configure GPIO pins : SPI_HOLD_Pin SPI_CS_Pin SPI_WP_Pin */
  GPIO_InitStruct.Pin = SPI_HOLD_Pin|SPI_CS_Pin|SPI_WP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PC8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}


/* USER CODE BEGIN 4 */

void LIS2DH12_Init(void)
{
    uint8_t data;

    // 100Hz, XYZ enable
    data = 0x57;
    HAL_I2C_Mem_Write(&hi2c1,
                      LIS2DH12_ADDR,
                      CTRL_REG1,
                      I2C_MEMADD_SIZE_8BIT,
                      &data,
                      1,
                      HAL_MAX_DELAY);

    // High resolution, BDU enable, ±2g
    data = 0x88;
    HAL_I2C_Mem_Write(&hi2c1,
                      LIS2DH12_ADDR,
                      0x23,
                      I2C_MEMADD_SIZE_8BIT,
                      &data,
                      1,
                      HAL_MAX_DELAY);
}


void LIS2DH12_Read(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t rawData[6];
    int16_t raw_x, raw_y, raw_z;

    HAL_I2C_Mem_Read(&hi2c1,
                     LIS2DH12_ADDR,
                     OUT_X_L | 0x80,
                     I2C_MEMADD_SIZE_8BIT,
                     rawData,
                     6,
                     HAL_MAX_DELAY);

    raw_x = (int16_t)((rawData[1] << 8) | rawData[0]);
    raw_y = (int16_t)((rawData[3] << 8) | rawData[2]);
    raw_z = (int16_t)((rawData[5] << 8) | rawData[4]);

    // 12-bit data in high-resolution mode
    raw_x = raw_x / 16;
    raw_y = raw_y / 16;
    raw_z = raw_z / 16;

    *x = raw_x;
    *y = raw_y;
    *z = raw_z;
}
double NMEA_To_Decimal(char *coord)
{
    double value;
    int degrees;
    double minutes;

    value = atof(coord);

    degrees = (int)(value / 100);

    minutes = value - (degrees * 100);

    return degrees + (minutes / 60.0);
}




void Parse_GNRMC(char *sentence)
{
    char temp[150];

    strcpy(temp, sentence);

    char *token;
    uint8_t field = 0;

    char raw_time[20] = {0};

    char raw_lat[20] = {0};
    char lat_dir = 'N';

    char raw_lon[20] = {0};
    char lon_dir = 'E';

    char *ptr = temp;


    /* -----------------------------------------
       Extract required fields from GNRMC
       ----------------------------------------- */

    while((token = strsep(&ptr, ",")) != NULL)
    {
        switch(field)
        {
            /* UTC Time */
            case 1:

                strcpy(raw_time, token);

                break;


            /* Latitude */
            case 3:

                strcpy(raw_lat, token);

                break;


            /* Latitude Direction */
            case 4:

                lat_dir = token[0];

                break;


            /* Longitude */
            case 5:

                strcpy(raw_lon, token);

                break;


            /* Longitude Direction */
            case 6:

                lon_dir = token[0];

                break;


            /* UTC Date */
            case 9:

                if(strlen(token) > 0)
                {
                    strcpy(gps_date, token);
                }

                break;
        }

        field++;
    }


    /* -----------------------------------------
       Convert UTC Time to IST
       ----------------------------------------- */

    int hh;
    int mm;
    int ss;

    sscanf(raw_time,
           "%2d%2d%2d",
           &hh,
           &mm,
           &ss);


    /* Add 5 hours 30 minutes */

    hh += 5;
    mm += 30;


    /* Handle minute overflow */

    if(mm >= 60)
    {
        mm -= 60;
        hh++;
    }


    /* -----------------------------------------
       Handle IST Date Change
       ----------------------------------------- */

    if(hh >= 24)
    {
        hh -= 24;


        /*
         * IST has moved to the next date.
         *
         * gps_date format:
         * DDMMYY
         */

        int day;
        int month;
        int year;
        int daysInMonth;


        sscanf(gps_date,
               "%2d%2d%2d",
               &day,
               &month,
               &year);


        /* Find number of days in current month */

        if(month == 2)
        {
            /* Leap year */

            if((year % 4) == 0)
            {
                daysInMonth = 29;
            }
            else
            {
                daysInMonth = 28;
            }
        }
        else if(month == 4 ||
                month == 6 ||
                month == 9 ||
                month == 11)
        {
            daysInMonth = 30;
        }
        else
        {
            daysInMonth = 31;
        }


        /* Move to next day */

        day++;


        /* If month is completed */

        if(day > daysInMonth)
        {
            day = 1;
            month++;


            /* If year is completed */

            if(month > 12)
            {
                month = 1;
                year++;


                if(year > 99)
                {
                    year = 0;
                }
            }
        }


        /* Store updated date */

        sprintf(gps_date,
                "%02d%02d%02d",
                day,
                month,
                year);
    }


    /* -----------------------------------------
       Store IST Time
       ----------------------------------------- */

    sprintf(gps_time_ist,
            "%02d:%02d:%02d",
            hh,
            mm,
            ss);


    /* -----------------------------------------
       Convert Latitude
       ----------------------------------------- */

    if(strlen(raw_lat) > 0)
    {
        latitude_dd = NMEA_To_Decimal(raw_lat);

        if(lat_dir == 'S')
        {
            latitude_dd = -latitude_dd;
        }
    }


    /* -----------------------------------------
       Convert Longitude
       ----------------------------------------- */

    if(strlen(raw_lon) > 0)
    {
        longitude_dd = NMEA_To_Decimal(raw_lon);

        if(lon_dir == 'W')
        {
            longitude_dd = -longitude_dd;
        }
    }
}




void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == UART5)
	{
	    cmd_char = uart_rx_char;      // Copy received byte
	    cmd_received = 1;

	    HAL_UART_Receive_IT(&huart5, &uart_rx_char, 1);
	}
    if (huart->Instance == USART3)
    {
        if (rx_char == '$')
        {
            rx_index = 0;
            rx_buffer[rx_index++] = rx_char;
        }
        else if (rx_char == '\n' || rx_char == '\r')
        {
            rx_buffer[rx_index] = '\0';

            if (strstr(rx_buffer, "$GNRMC"))
            {
                Parse_GNRMC(rx_buffer);
            }

            rx_index = 0;
        }
        else
        {
            if (rx_index < RX_BUFFER_SIZE - 1)
            {
                rx_buffer[rx_index++] = rx_char;
            }
        }

        HAL_UART_Receive_IT(&huart3, &rx_char, 1);
    }
}

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
#ifdef USE_FULL_ASSERT
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
