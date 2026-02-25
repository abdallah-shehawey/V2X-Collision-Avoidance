/**
 * @file MPU9250_Test_main.c
 * @author Abdallah Abdelmoemen Shehawey
 * @brief Example code to test and demonstrate MPU9250 driver functionality
 * @details This code initializes the sensor and calculates orientation, speed, and position.
 */

#include "../../MCAL/RCC/Inc/RCC_interface.h"
#include "../../MCAL/GPIO/Inc/GPIO_interface.h"
#include "../../MCAL/SPI/Inc/SPI_interface.h"
#include "../../MCAL/TIM/Inc/TIM_interface.h"
#include "Inc/MPU9250_interface.h"
#include "Inc/MPU9250_config.h"

/* Global variables to observe data in Debug mode */
MPU9250_Data_t     SensorData;
MPU9250_Position_t CurrentPos = {0.0f, 0.0f, 0.0f};
float              Heading = 0.0f;
float              Pitch = 0.0f, Roll = 0.0f;
float              Speed = 0.0f;
float              dt = 0.01f; /* 10ms loop time */

void App_vInitHardware(void);

int main(void)
{
    /* 1. Initialize System Clocks and Hardware Pins */
    App_vInitHardware();

    /* 2. Initialize MPU9250 Sensor */
    if (MPU9250_enumInit() == OK)
    {
        /* Initialization success - sensor is ready */
        
        while (1)
        {
            /* 3. Read raw processed data (Accel, Gyro, Mag, Temp) */
            if (MPU9250_enumReadData(&SensorData) == OK)
            {
                /* 4. Calculate 2D Heading (Orientation) */
                MPU9250_enumGetHeading(&SensorData, &Heading);

                /* 5. Calculate Attitude (Pitch and Roll) */
                MPU9250_enumGetAttitude(&SensorData, &Pitch, &Roll);

                /* 6. Estimate Linear Speed (Integration of AccelX) */
                MPU9250_enumGetSpeed(&SensorData, dt, &Speed);

                /* 7. Update 3D Position (Dead Reckoning) */
                MPU9250_enumGetPosition(Speed, Heading, Pitch, dt, &CurrentPos);
            }

            /* 8. Loop Delay (e.g., 10ms for 100Hz update rate) */
            TIM_vDelayMs(MPU9250_DELAY_TIMER, 10);
        }
    }
    else
    {
        /* Initialization failed - check wiring or SPI configuration */
        while(1);
    }
}

/**
 * @brief Configures RCC and GPIO pins for SPI1 and MPU9250.
 */
void App_vInitHardware(void)
{
    /* Enable Peripherals Clocks */
    RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOAEN, RCC_PER_ON);  /* GPIOA for SPI1 & CS */
    RCC_enumABPPerSts(RCC_APB2, RCC_SPI1EN, RCC_PER_ON);   /* SPI1 */
    RCC_enumABPPerSts(RCC_APB1, RCC_TIM6EN, RCC_PER_ON);   /* Timer 6 for delays */

    /* Configure SPI1 Pins: PA5 (SCK), PA6 (MISO), PA7 (MOSI) */
    GPIO_PinConfig_t SPI_Pins = {
        .Port = GPIO_PORTA,
        .Mode = GPIO_ALTFN,
        .Otype = GPIO_PUSH_PULL,
        .Speed = GPIO_VERY_HIGH_SPEED,
        .PullType = GPIO_NO_PULL,
        .AlternateFunction = GPIO_AF5 /* AF5 for SPI1 */
    };

    SPI_Pins.PinNum = GPIO_PIN5;
    GPIO_enumPinInit(&SPI_Pins);
    SPI_Pins.PinNum = GPIO_PIN6;
    GPIO_enumPinInit(&SPI_Pins);
    SPI_Pins.PinNum = GPIO_PIN7;
    GPIO_enumPinInit(&SPI_Pins);
    
    /* CS Pin (PA4) is initialized inside MPU9250_enumInit() per driver implementation */
}
