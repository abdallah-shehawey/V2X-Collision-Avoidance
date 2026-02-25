/*
 * HSTP_config.h
 *
 *  Created on: Aug 3, 2025
 *      Author: abdallah-shehawey
 */

#ifndef HSTP_CONFIG_H_
#define HSTP_CONFIG_H_


#define HSTP_DS_PORT GPIO_PORTA
#define HSTP_DS_PIN GPIO_PIN0

#define HSTP_SHCR_PORT GPIO_PORTA
#define HSTP_SHCR_PIN GPIO_PIN1

#define HSTP_STCR_PORT GPIO_PORTA
#define HSTP_STCR_PIN GPIO_PIN2



GPIO_PinConfig_t HSTP_DS =
{
    .Port = HSTP_DS_PORT,
    .PinNum = HSTP_DS_PIN,
    .Mode = GPIO_OUTPUT,
    .Otype = GPIO_PUSH_PULL,
    .Speed = GPIO_MEDIUM_SPEED,
    .PullType = GPIO_NO_PULL,
};

GPIO_PinConfig_t HSTP_SHCR =
{
    .Port = HSTP_SHCR_PORT,
    .PinNum = HSTP_SHCR_PIN,
    .Mode = GPIO_OUTPUT,
    .Otype = GPIO_PUSH_PULL,
    .Speed = GPIO_MEDIUM_SPEED,
    .PullType = GPIO_NO_PULL,
};

GPIO_PinConfig_t HSTP_STCR =
{
    .Port = HSTP_STCR_PORT,
    .PinNum = HSTP_STCR_PIN,
    .Mode = GPIO_OUTPUT,
    .Otype = GPIO_PUSH_PULL,
    .Speed = GPIO_MEDIUM_SPEED,
    .PullType = GPIO_NO_PULL,
};


#endif /* HSTP_CONFIG_H_ */
