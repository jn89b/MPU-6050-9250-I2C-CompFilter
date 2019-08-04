#include "mpu9250.h"

#ifdef ARDUINO
#include <arduino.h>
#else
#include <math.h>
#endif

MPU9250::MPU9250(unsigned char addr, i2c_device_t i2c_dev){
  _addr = addr;
  _i2c_dev = i2c_dev;
}

bool MPU9250::initIMU(int sensor) {
  // Check if a valid connection has been established
  data[0] = WHO_AM_I;
  _i2c_dev.i2c_write(_addr, data, 1);
  _i2c_dev.i2c_read(_addr, data, 1);
  unsigned char whoAmI = data[0];

  if ( (whoAmI == 0x68 && sensor == MPU6050) || (whoAmI == 0x71 && sensor == MPU9250) ){
    // Activate/reset the IMU
    write2bytes(PWR_MGMT_1, 0x00);
    return true;
  }

  return false;
}

int MPU9250::write2bytes(unsigned char byte0, unsigned char byte1) {
  data[0] = byte0;
  data[1] = byte1;
  _i2c_dev.i2c_write(_addr, data, 2);
}

float MPU9250::getAres(int Ascale) {
  // Set the full scale range for the accelerometer
  switch (Ascale){
    case AFS_2G:
      _aRes = 16384.0;
      write2bytes(ACCEL_CONFIG, 0x00);
      return _aRes;
    case AFS_4G:
      _aRes = 8192.0;
      write2bytes(ACCEL_CONFIG, 0x08);
      return _aRes;
    case AFS_8G:
      _aRes = 4096.0;
      write2bytes(ACCEL_CONFIG, 0x10);
      return _aRes;
    case AFS_16G:
      _aRes = 2048.0;
      write2bytes(ACCEL_CONFIG, 0x18);
      return _aRes;
    default:
      return 0;
  }
}

float MPU9250::getGres(int Gscale) {
  // Set the full scale range for the gyroscope
  switch (Gscale){
    case GFS_250DPS:
      _gRes = 131.0;
      write2bytes(GYRO_CONFIG, 0x00);
      return _gRes;
    case GFS_500DPS:
      _gRes = 65.5;
      write2bytes(GYRO_CONFIG, 0x08);
      return _gRes;
    case GFS_1000DPS:
      _gRes = 32.8;
      write2bytes(GYRO_CONFIG, 0x10);
      return _gRes;
    case GFS_2000DPS:
      _gRes = 16.4;
      write2bytes(GYRO_CONFIG, 0x18);
      return _gRes;
    default:
      return 0;
  }
}

void MPU9250::readRawData() {
  // Subroutine for reading the raw data
  data[0] = ACCEL_XOUT_H;
  _i2c_dev.i2c_write(_addr, data, 1);
  _i2c_dev.i2c_read(_addr, data, 14);

  // Read data - Temperature falls between accel and gyro registers
  imu_raw.ax = (short) (data[0]  << 8 | data[1]);
  imu_raw.ay = (short) (data[2]  << 8 | data[3]);
  imu_raw.az = (short) (data[4]  << 8 | data[5]);

  imu_raw.gx = (short) (data[8]  << 8 | data[9]);
  imu_raw.gy = (short) (data[10] << 8 | data[11]);
  imu_raw.gz = (short) (data[12] << 8 | data[13]);

  temperature = (short) (data[6] << 8 | data[7]);
}

void MPU9250::readCalData() {
  // Get new data
  readRawData();

  // Convert accelerometer values to g's
  imu_cal.ax = imu_raw.ax / _aRes;
  imu_cal.ay = imu_raw.ay / _aRes;
  imu_cal.az = imu_raw.az / _aRes;

  // Remove gyro offset
  imu_cal.gx = imu_raw.gx - gyro_cal.x;
  imu_cal.gy = imu_raw.gy - gyro_cal.y;
  imu_cal.gz = imu_raw.gz - gyro_cal.z;

  // Convert gyro values to degrees per second
  imu_cal.gx /= _gRes;
  imu_cal.gy /= _gRes;
  imu_cal.gz /= _gRes;
}

void MPU9250::compFilter(float dt, float tau) {
  // Read calibrated data
  readCalData();

  // Complementary filter
  accelPitch = atan2(imu_cal.ay, imu_cal.az) * (180 / M_PI);
  accelRoll = atan2(imu_cal.ax, imu_cal.az) * (180 / M_PI);

  attitude.roll = (tau)*(attitude.roll - imu_cal.gy*dt) + (1-tau)*(accelRoll);
  attitude.pitch = (tau)*(attitude.pitch + imu_cal.gx*dt) + (1-tau)*(accelPitch);
  attitude.yaw = imu_cal.gz*dt;
}

void MPU9250::gyroCalibration(int numCalPoints) {
  // Initialize standard deviation and check variabls
  float stdX, stdY, stdZ;
  float xCheckL, yCheckL, zCheckL;
  float xCheckH, yCheckH, zCheckH;
  float numeratorX, numeratorY, numeratorZ;

  // Run calibration for given number of points
  for (int ii = 0; ii < numCalPoints; ii++){
    readRawData();
    gyro_cal.x += imu_raw.gx;
    gyro_cal.y += imu_raw.gy;
    gyro_cal.z += imu_raw.gz;
  }

  // Find the averge offset values
  gyro_cal.x /= (float)numCalPoints;
  gyro_cal.y /= (float)numCalPoints;
  gyro_cal.z /= (float)numCalPoints;
}

void MPU9250::setGyroCalibration(gyro_cal_t gyro) {
  gyro_cal = gyro;
}

gyro_cal_t MPU9250::getGyroCalibration() {
  return gyro_cal;
}
