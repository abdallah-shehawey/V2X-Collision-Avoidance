/**
 * @file MPU9250_program.c
 * @author Abdallah Saleh
 * @brief Professional MPU9250 driver: High-Accuracy Speed (m/s) & Gravity-Compensated Altitude
 */

#include "../Inc/Drivers/HAL/MPU9250/MPU9250_interface.h"
#include "../Inc/Drivers/HAL/MPU9250/MPU9250_private.h"
#include "../Inc/Drivers/HAL/MPU9250/MPU9250_config.h"
#include "../Inc/Drivers/MCAL/SPI/SPI_interface.h"
#include "../Inc/Drivers/MCAL/GPIO/GPIO_interface.h"
#include "../Inc/Drivers/MCAL/TIM/TIM_interface.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Global State */
static float Accel_Offset[3] = {0, 0, 0};
static float Gyro_Offset[3]  = {0, 0, 0};
static float Mag_ASA[3]      = {1.0f, 1.0f, 1.0f}; /* AK8963 factory sensitivity adjust */
/* Continuous hard-iron auto-calibration: running field extremes + bias offset */
static float Mag_MinX = 1.0e9f, Mag_MaxX = -1.0e9f;
static float Mag_MinY = 1.0e9f, Mag_MaxY = -1.0e9f;
static float Mag_OffX = 0.0f,   Mag_OffY = 0.0f;
static float Filtered_Pitch = 0, Filtered_Roll = 0, Filtered_Heading = 0;
static float Vert_Velocity_m_s = 0; 
volatile uint8_t MPU9250_ID = 0; /* Visible ID for debugging */

/* SPI Config */
static SPI_Config_t MPU9250_SPI = {
    .Channel = MPU9250_SPI_CHANNEL, .CPHA = SPI_CPHA_2EDGE, .CPOL = SPI_CPOL_HIGH,
    .Mode = SPI_MODE_MASTER, .BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64,
    .SPE = SPI_SPE_EN, .BFIRST = SPI_MSBFIRST, .NSS_MAN = SPI_NSS_SOFTWARE,
    .NSSI_MODE = SPI_NSSI_NOT_SELECT, /* Fixed Master Mode Fault */
    .DFF = SPI_DFF_8BIT, .CRC_MODE = SPI_CRCDIS
};

/* Helpers */
static void MPU9250_vSelect(void)   { GPIO_enumWritePinVal(MPU9250_CS_PORT, MPU9250_CS_PIN, GPIO_PIN_LOW); }
static void MPU9250_vDeselect(void) { GPIO_enumWritePinVal(MPU9250_CS_PORT, MPU9250_CS_PIN, GPIO_PIN_HIGH); }

static void MPU9250_vWriteReg(uint8_t reg, uint8_t data) {
  uint16_t d; MPU9250_vSelect();
  SPI_enumTrancieve(&MPU9250_SPI, (uint16_t)reg, &d);
  SPI_enumTrancieve(&MPU9250_SPI, (uint16_t)data, &d);
  MPU9250_vDeselect();
}

static uint8_t MPU9250_u8ReadReg(uint8_t reg) {
  uint16_t val; MPU9250_vSelect();
  SPI_enumTrancieve(&MPU9250_SPI, (uint16_t)(reg | 0x80), &val);
  SPI_enumTrancieve(&MPU9250_SPI, 0x00, &val);
  MPU9250_vDeselect();
  return (uint8_t)val;
}

/* Read one AK8963 register through the MPU9250 internal I2C master.
 * The fetched byte lands in EXT_SENS_DATA_00 once the master completes. */
static uint8_t MPU9250_u8ReadMagReg(uint8_t reg) {
  MPU9250_vWriteReg(I2C_SLV0_ADDR, 0x80 | AK8963_ADDR); /* AK8963 addr, read */
  MPU9250_vWriteReg(I2C_SLV0_REG,  reg);
  MPU9250_vWriteReg(I2C_SLV0_CTRL, 0x81);               /* enable, 1 byte */
  TIM_vDelayMs(MPU9250_DELAY_TIMER, 10);
  return MPU9250_u8ReadReg(EXT_SENS_DATA_00);
}

static void MPU9250_vInitMag(void) {
  /* SPI-only (I2C_IF_DIS) + enable internal I2C master @400kHz, no bypass */
  MPU9250_vWriteReg(0x37, 0x00); TIM_vDelayMs(MPU9250_DELAY_TIMER, 10);
  MPU9250_vWriteReg(0x6A, 0x30); MPU9250_vWriteReg(0x24, 0x0D);

  /* ── Read factory sensitivity adjustment (ASA) from the AK8963 fuse ROM ──
   * Sequence per datasheet: power-down -> fuse-ROM access -> read ASA ->
   * power-down. Applied per-axis as a scale in MPU9250_enumReadData. */
  MPU9250_vWriteReg(I2C_SLV0_ADDR, AK8963_ADDR);   /* AK8963 addr, write */
  MPU9250_vWriteReg(I2C_SLV0_REG,  AK8963_CNTL);   /* CNTL1 */
  MPU9250_vWriteReg(I2C_SLV0_DO,   0x00);          /* power-down */
  MPU9250_vWriteReg(I2C_SLV0_CTRL, 0x81);
  TIM_vDelayMs(MPU9250_DELAY_TIMER, 10);

  MPU9250_vWriteReg(I2C_SLV0_DO,   0x0F);          /* fuse-ROM access mode */
  MPU9250_vWriteReg(I2C_SLV0_CTRL, 0x81);
  TIM_vDelayMs(MPU9250_DELAY_TIMER, 10);

  uint8_t asa_x = MPU9250_u8ReadMagReg(AK8963_ASAX);
  uint8_t asa_y = MPU9250_u8ReadMagReg(AK8963_ASAY);
  uint8_t asa_z = MPU9250_u8ReadMagReg(AK8963_ASAZ);
  Mag_ASA[0] = ((float)asa_x - 128.0f) / 256.0f + 1.0f;
  Mag_ASA[1] = ((float)asa_y - 128.0f) / 256.0f + 1.0f;
  Mag_ASA[2] = ((float)asa_z - 128.0f) / 256.0f + 1.0f;

  MPU9250_vWriteReg(I2C_SLV0_ADDR, AK8963_ADDR);   /* back to write mode */
  MPU9250_vWriteReg(I2C_SLV0_REG,  AK8963_CNTL);
  MPU9250_vWriteReg(I2C_SLV0_DO,   0x00);          /* power-down */
  MPU9250_vWriteReg(I2C_SLV0_CTRL, 0x81);
  TIM_vDelayMs(MPU9250_DELAY_TIMER, 10);

  /* Put the AK8963 straight into continuous mode 2 (100 Hz), 16-bit output
   * (CNTL1 = 0x16) via the internal I2C master. */
  MPU9250_vWriteReg(I2C_SLV0_ADDR, 0x0C); /* AK8963 I2C addr, write */
  MPU9250_vWriteReg(I2C_SLV0_REG,  0x0A); /* CNTL1 */
  MPU9250_vWriteReg(I2C_SLV0_DO,   0x16);
  MPU9250_vWriteReg(I2C_SLV0_CTRL, 0x81);
  TIM_vDelayMs(MPU9250_DELAY_TIMER, 10);

  /* Stream 7 bytes (HXL..HZH + ST2) from AK8963 reg 0x03 every sample.
   * ST2 MUST be in the read or the AK8963 latches and stops updating. */
  MPU9250_vWriteReg(I2C_SLV0_ADDR, 0x8C); /* AK8963 I2C addr, read */
  MPU9250_vWriteReg(I2C_SLV0_REG,  0x03); /* HXL */
  MPU9250_vWriteReg(I2C_SLV0_CTRL, 0x87); /* enable, 7 bytes */
}

ErrorState_t MPU9250_enumInit(void) {
  GPIO_PinConfig_t CS = {.Port=MPU9250_CS_PORT, .PinNum=MPU9250_CS_PIN, .Mode=GPIO_OUTPUT, .Speed=GPIO_VERY_HIGH_SPEED};
  GPIO_enumPinInit(&CS); MPU9250_vDeselect();
  SPI_enumInit(&MPU9250_SPI);

  MPU9250_vWriteReg(0x6B, 0x80); TIM_vDelayMs(MPU9250_DELAY_TIMER, 150);
  MPU9250_vWriteReg(0x6B, 0x01); TIM_vDelayMs(MPU9250_DELAY_TIMER, 50);
  MPU9250_vWriteReg(0x1A, 0x03);

  MPU9250_ID = MPU9250_u8ReadReg(0x75);
  MPU9250_vInitMag();

  float sum[6] = {0};
  for(int i=0; i<400; i++) {
    uint8_t b[14]; uint16_t v;
    MPU9250_vSelect(); SPI_enumTrancieve(&MPU9250_SPI, 0xBB, &v);
    for(int j=0; j<14; j++) { SPI_enumTrancieve(&MPU9250_SPI, 0, &v); b[j]=(uint8_t)v; }
    MPU9250_vDeselect();
    sum[0] += (int16_t)((b[0]<<8)|b[1]); sum[1] += (int16_t)((b[2]<<8)|b[3]); sum[2] += (int16_t)((b[4]<<8)|b[5]);
    sum[3] += (int16_t)((b[8]<<8)|b[9]); sum[4] += (int16_t)((b[10]<<8)|b[11]); sum[5] += (int16_t)((b[12]<<8)|b[13]);
    TIM_vDelayMs(MPU9250_DELAY_TIMER, 2);
  }
  for(int i=0; i<2; i++) Accel_Offset[i] = sum[i]/400.0f;
  Accel_Offset[2] = (sum[2]/400.0f) - 16384.0f;
  for(int i=0; i<3; i++) Gyro_Offset[i]  = sum[i+3]/400.0f;

  MPU9250_SPI.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  SPI_enumInit(&MPU9250_SPI);
  return OK;
}

ErrorState_t MPU9250_enumReadData(MPU9250_Data_t *D) {
  if (!D) return NULL_POINTER;
  uint8_t buffer[21]; uint16_t val;
  MPU9250_vSelect(); SPI_enumTrancieve(&MPU9250_SPI, 0xBB, &val);
  for(int i=0; i<21; i++) { SPI_enumTrancieve(&MPU9250_SPI, 0, &val); buffer[i] = (uint8_t)val; }
  MPU9250_vDeselect();
  D->AccelX = ((float)((int16_t)((buffer[0]<<8)|buffer[1])) - Accel_Offset[0]) / 16384.0f;
  D->AccelY = ((float)((int16_t)((buffer[2]<<8)|buffer[3])) - Accel_Offset[1]) / 16384.0f;
  D->AccelZ = ((float)((int16_t)((buffer[4]<<8)|buffer[5])) - Accel_Offset[2]) / 16384.0f;
  D->GyroX = ((float)((int16_t)((buffer[8]<<8)|buffer[9])) - Gyro_Offset[0]) / 131.0f;
  D->GyroY = ((float)((int16_t)((buffer[10]<<8)|buffer[11])) - Gyro_Offset[1]) / 131.0f;
  D->GyroZ = ((float)((int16_t)((buffer[12]<<8)|buffer[13])) - Gyro_Offset[2]) / 131.0f;
  D->MagX = (float)((int16_t)((buffer[15]<<8)|buffer[14])) * Mag_ASA[0];
  D->MagY = (float)((int16_t)((buffer[17]<<8)|buffer[16])) * Mag_ASA[1];
  D->MagZ = (float)((int16_t)((buffer[19]<<8)|buffer[18])) * Mag_ASA[2];
  return OK;
}

ErrorState_t MPU9250_enumGetAttitude(MPU9250_Data_t *D, float dt, float *P, float *R) {
  if (!D || !P || !R) return NULL_POINTER;
  float accP = atan2f(-D->AccelX, sqrtf(D->AccelY*D->AccelY + D->AccelZ*D->AccelZ)) * (180.0f/M_PI);
  float accR = atan2f(D->AccelY, D->AccelZ) * (180.0f/M_PI);

  /* Fast Response Filter (0.95) for better Z tracking */
      Filtered_Pitch = 0.95f * (Filtered_Pitch + D->GyroY * dt) + 0.05f * accP;
      Filtered_Roll  = 0.95f * (Filtered_Roll  + D->GyroX * dt) + 0.05f * accR;

      *P = Filtered_Pitch; *R = Filtered_Roll;
      return OK;
}

ErrorState_t MPU9250_enumGetHeading(MPU9250_Data_t *D, float *h) {
  if (!D || !h) return NULL_POINTER;

  /* ── Continuous hard-iron auto-calibration ──
   * Track the running min/max of the (ASA-corrected) horizontal field as the
   * vehicle turns; the hard-iron bias is the midpoint of each axis' swing.
   * The estimate sharpens automatically the more the car turns. */
  if (D->MagX < Mag_MinX) Mag_MinX = D->MagX;
  if (D->MagX > Mag_MaxX) Mag_MaxX = D->MagX;
  if (D->MagY < Mag_MinY) Mag_MinY = D->MagY;
  if (D->MagY > Mag_MaxY) Mag_MaxY = D->MagY;

  /* Relax the extremes inward slightly so a one-off spike decays out over time
   * and the calibration stays "live" rather than latching forever. */
  Mag_MaxX -= MPU9250_MAG_DECAY; Mag_MinX += MPU9250_MAG_DECAY;
  Mag_MaxY -= MPU9250_MAG_DECAY; Mag_MinY += MPU9250_MAG_DECAY;

  /* Trust the offset only once the field span on BOTH axes is wide enough to
   * mean the car has actually turned. Until then the offset keeps its last
   * good value (0 at startup -> raw, uncorrected heading as a safe fallback). */
  if ((Mag_MaxX - Mag_MinX) > MPU9250_MAG_MIN_SPAN &&
      (Mag_MaxY - Mag_MinY) > MPU9250_MAG_MIN_SPAN) {
    Mag_OffX = 0.5f * (Mag_MaxX + Mag_MinX);
    Mag_OffY = 0.5f * (Mag_MaxY + Mag_MinY);
  }

  float mx = D->MagX - Mag_OffX;
  float my = D->MagY - Mag_OffY;
  float rawH = atan2f(my, mx) * (180.0f/M_PI);
  if(rawH < 0) rawH += 360.0f;
  Filtered_Heading = 0.9f * Filtered_Heading + 0.1f * rawH;
  *h = Filtered_Heading; return OK;
}

ErrorState_t MPU9250_enumGetSpeed(MPU9250_Data_t *D, float dt, float *s) {
  if (!D || !s) return NULL_POINTER;
  
  float total_acc = sqrtf(D->AccelX*D->AccelX + D->AccelY*D->AccelY + D->AccelZ*D->AccelZ);
  /* Convert G to m/s^2 (1g = 9.80665 m/s^2) */
  float linear_acc_m_s2 = (total_acc - 1.0f) * 9.80665f;

  /* 1. Enhanced ZUPT Logic (Stillness Detection) */
  /* If Gyro noise and Accel variation are below thresholds, force deceleration */
  if (fabs(D->GyroX) < 1.5f && fabs(D->GyroY) < 1.5f && fabs(D->GyroZ) < 1.5f) {
    if (fabs(linear_acc_m_s2) < 0.35f) { 
      /* Apply strong damping to bleed off drift speed */
      *s *= 0.70f; 
      if (fabs(*s) < 0.05f) *s = 0.0f;
      linear_acc_m_s2 = 0.0f;
    }
  }

  /* 2. Deadzone to ignore sensor floor noise */
  if (fabs(linear_acc_m_s2) < 0.20f) {
      linear_acc_m_s2 = 0.0f;
  }

  /* 3. Trapezoidal Integration */
  *s += linear_acc_m_s2 * dt;

  /* 4. Physical Limit: Speed cannot be negative in this integration model */
  if (*s < 0) *s = 0;
  if (*s < 0.015f) *s = 0; /* 1.5 cm/s clamp in meters */

  return OK;
}

ErrorState_t MPU9250_enumGetPosition(MPU9250_Data_t *D, float s, float h, float pit, float dt, MPU9250_Position_t *pos) {
  if (!D || !pos) return NULL_POINTER;

  float radP = Filtered_Pitch * (M_PI/180.0f);
  float radR = Filtered_Roll * (M_PI/180.0f);

  /* Gravity-Compensated World Acceleration in m/s^2 */
  float az_world = (D->AccelZ * cosf(radP) * cosf(radR)) - (D->AccelX * sinf(radP)) + (D->AccelY * sinf(radR) * cosf(radP)) - 1.0f;
  float az_m_s2 = az_world * 9.80665f;

  /* Smart Vertical Stillness Detection */
  float total_gyro = fabs(D->GyroX) + fabs(D->GyroY) + fabs(D->GyroZ);
  if (total_gyro < 1.5f && fabs(az_m_s2) < 0.35f) {
    az_m_s2 = 0;
    Vert_Velocity_m_s *= 0.75f; /* Snap speed to zero when stationary */
  }

  /* Leaky Integration to fix the Negative Drifting Issue */
  if (fabs(az_m_s2) > 0.20f) {
    Vert_Velocity_m_s += az_m_s2 * dt;
  }
  Vert_Velocity_m_s *= 0.97f; /* Continuous bleed to stabilize at rest */

  /* Update Distance Z in Meters */
  pos->Z += Vert_Velocity_m_s * dt;

  /* When stationary: bleed Z back toward 0 (reference point)
   * Avoids drift accumulation — at 50ms cycles: ~5s to converge back to 0 */
  if (total_gyro < 1.5f && fabs(Vert_Velocity_m_s) < 0.01f) {
    pos->Z *= 0.98f;
    if (fabs(pos->Z) < 0.005f) pos->Z = 0.0f;  /* snap at 5mm */
  }

  pos->X = 0; pos->Y = 0;
  return OK;
}
