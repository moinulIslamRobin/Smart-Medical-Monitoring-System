#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>

// Time calculation
int timezone = 6 * 3600;
int dst = 0;

// Gyro Start
const uint8_t MPU_addr = 0x68; // I2C address of the MPU-6050

const float MPU_GYRO_250_SCALE = 131.0;
const float MPU_GYRO_500_SCALE = 65.5;
const float MPU_GYRO_1000_SCALE = 32.8;
const float MPU_GYRO_2000_SCALE = 16.4;
const float MPU_ACCL_2_SCALE = 16384.0;
const float MPU_ACCL_4_SCALE = 8192.0;
const float MPU_ACCL_8_SCALE = 4096.0;
const float MPU_ACCL_16_SCALE = 2048.0;

struct rawdata {
  int16_t AcX;
  int16_t AcY;
  int16_t AcZ;
  int16_t Tmp;
  int16_t GyX;
  int16_t GyY;
  int16_t GyZ;
};

struct scaleddata {
  float AcX;
  float AcY;
  float AcZ;
  float Tmp;
  float GyX;
  float GyY;
  float GyZ;
};

bool checkI2c(byte addr);
void mpu6050Begin(byte addr);
rawdata mpu6050Read(byte addr, bool Debug);
void setMPU6050scales(byte addr, uint8_t Gyro, uint8_t Accl);
void getMPU6050scales(byte addr, uint8_t & Gyro, uint8_t & Accl);
scaleddata convertRawToScaled(byte addr, rawdata data_in, bool Debug);
// Gyro End

void setup() {
  Serial.begin(9600);
  pinMode(D3, INPUT);
  pinMode(D4, INPUT);

  // WiFi.begin("AndroidAP safat", "trexis-awesome");   //WiFi connection
  WiFi.begin("NETGEAR00", "blackcomet097");   //WiFi connection

  while (WiFi.status() != WL_CONNECTED) {  // Wait for the WiFI connection completion
    delay(500);
    Serial.println("Waiting for connection");
  }
  Serial.println("WiFi Connected!");
  configTime(timezone, dst, "pool.ntp.org","time.nist.gov");

  // MPU initiate
  Wire.begin();
  mpu6050Begin(MPU_addr);
}

void loop() {
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  String day = String(p_tm->tm_mday);
  String mon = String(p_tm->tm_mon+1);
  String year = String(p_tm->tm_year + 1900);
  String hour = String(p_tm->tm_hour);
  String mint = String(p_tm->tm_min);
  String sec = String(p_tm->tm_sec);
  String curTime = hour+":"+mint+":"+sec;
  String dateTime = year+"-"+mon+"-"+day+" "+curTime;
  // Serial.print(dateTime);

  if((digitalRead(10) == 1)||(digitalRead(11) == 1)){
    Serial.println('!');
  }
  else{
    // Send the value of analog input 0:
    Serial.println(analogRead(A0));
  }
  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    StaticJsonBuffer<300> JSONbuffer;   //Declaring static JSON buffer
    JsonObject& ecgJson = JSONbuffer.createObject();

    ecgJson["Device"] = "ECG";

    JsonArray& ecgValues = ecgJson.createNestedArray("Value");
    JsonArray& ecgTimestamp = ecgJson.createNestedArray("Timestamp");


    ecgValues.add(analogRead(A0)); // Add ECG value to array
    ecgTimestamp.add(dateTime); //Add Date and Time to array

    char JSONmessageBuffer[300];
    ecgJson.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
    Serial.println(JSONmessageBuffer);

    HTTPClient http; //Declare object of class HTTPClient

    http.begin("http://104.197.51.157/smart_health/"); //Specify request destination
    http.addHeader("Content-Type", "application/json"); //Specify content-type header

    int httpCode = http.POST(JSONmessageBuffer); //Send the request
    String payload = http.getString(); //Get the response payload

    //Serial.println(httpCode); //Print HTTP return code
    //Serial.println(payload); //Print request response payload

    // Gyro Start
    rawdata next_sample;
    setMPU6050scales(MPU_addr, 0b00000000, 0b00010000);
    next_sample = mpu6050Read(MPU_addr, false);
    // convertRawToScaled(MPU_addr, next_sample, true);

    scaleddata scaledGyroValues = convertRawToScaled(MPU_addr, next_sample, false); // Debugging off

    //GyX = 0.21 °/s| GyY = -2.82 °/s| GyZ = -10.09 °/s| Tmp = 33.28 °C| AcX = -0.67 g| AcY = -0.77 g| AcZ = 0.11 g

    JSONbuffer.clear();
    JsonObject& gyroJson = JSONbuffer.createObject();
    gyroJson["Device"] = "Gyro";
    JsonArray& gyroValues = gyroJson.createNestedArray("Value");
    JsonArray& gyroTimestamp = gyroJson.createNestedArray("Timestamp");

    // Adding Gyro/Tmp/Acc Values to Array
    gyroValues.add( scaledGyroValues.GyX );
    gyroValues.add( scaledGyroValues.GyY );
    gyroValues.add( scaledGyroValues.GyZ );
    gyroValues.add( scaledGyroValues.Tmp );
    gyroValues.add( scaledGyroValues.AcX );
    gyroValues.add( scaledGyroValues.AcY );
    gyroValues.add( scaledGyroValues.AcZ );
    gyroTimestamp.add(dateTime); //Add Date and Time to array

    char JSONmessageBufferGyro[300];
    gyroJson.prettyPrintTo(JSONmessageBufferGyro, sizeof(JSONmessageBufferGyro));
    Serial.println(JSONmessageBufferGyro);

    int httpCode2 = http.POST(JSONmessageBufferGyro); //Send the request
    String payload2 = http.getString(); //Get the response payload

    //Serial.println(httpCode2); //Print HTTP return code
    //Serial.println(payload2); //Print request response payload

    // Gyro End

    http.end();  //Close connection
  } else {
    Serial.println("Error in WiFi connection");
  }
  delay(1000);
}

// Gyro Start

void mpu6050Begin(byte addr) {
  // This function initializes the MPU-6050 IMU Sensor
  // It verifys the address is correct and wakes up the
  // MPU.
  if (checkI2c(addr)) {
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x6B); // PWR_MGMT_1 register
    Wire.write(0); // set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);

    delay(30); // Ensure gyro has enough time to power up
  }
}

bool checkI2c(byte addr) {
  // We are using the return value of
  // the Write.endTransmisstion to see if
  // a device did acknowledge to the address.
  Serial.println(" ");
  Wire.beginTransmission(addr);

  if (Wire.endTransmission() == 0) {
    Serial.print(" Device Found at 0x");
    Serial.println(addr, HEX);
    return true;
  } else {
    Serial.print(" No Device Found at 0x");
    Serial.println(addr, HEX);
    return false;
  }
}

void setMPU6050scales(byte addr, uint8_t Gyro, uint8_t Accl) {
  Wire.beginTransmission(addr);
  Wire.write(0x1B); // write to register starting at 0x1B
  Wire.write(Gyro); // Self Tests Off and set Gyro FS to 250
  Wire.write(Accl); // Self Tests Off and set Accl FS to 8g
  Wire.endTransmission(true);
}

void getMPU6050scales(byte addr, uint8_t & Gyro, uint8_t & Accl) {
  Wire.beginTransmission(addr);
  Wire.write(0x1B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(addr, 2, true); // request a total of 14 registers
  Gyro = (Wire.read() & (bit(3) | bit(4))) >> 3;
  Accl = (Wire.read() & (bit(3) | bit(4))) >> 3;
}

scaleddata convertRawToScaled(byte addr, rawdata data_in, bool Debug) {

  scaleddata values;
  float scale_value = 0.0;
  byte Gyro, Accl;

  getMPU6050scales(MPU_addr, Gyro, Accl);

  if (Debug) {
    Serial.print("Gyro Full-Scale = ");
  }

  switch (Gyro) {
  case 0:
    scale_value = MPU_GYRO_250_SCALE;
    if (Debug) {
      Serial.println("±250 °/s");
    }
    break;
  case 1:
    scale_value = MPU_GYRO_500_SCALE;
    if (Debug) {
      Serial.println("±500 °/s");
    }
    break;
  case 2:
    scale_value = MPU_GYRO_1000_SCALE;
    if (Debug) {
      Serial.println("±1000 °/s");
    }
    break;
  case 3:
    scale_value = MPU_GYRO_2000_SCALE;
    if (Debug) {
      Serial.println("±2000 °/s");
    }
    break;
  default:
    break;
  }

  values.GyX = (float) data_in.GyX / scale_value;
  values.GyY = (float) data_in.GyY / scale_value;
  values.GyZ = (float) data_in.GyZ / scale_value;

  scale_value = 0.0;
  if (Debug) {
    Serial.print("Accl Full-Scale = ");
  }
  switch (Accl) {
  case 0:
    scale_value = MPU_ACCL_2_SCALE;
    if (Debug) {
      Serial.println("±2 g");
    }
    break;
  case 1:
    scale_value = MPU_ACCL_4_SCALE;
    if (Debug) {
      Serial.println("±4 g");
    }
    break;
  case 2:
    scale_value = MPU_ACCL_8_SCALE;
    if (Debug) {
      Serial.println("±8 g");
    }
    break;
  case 3:
    scale_value = MPU_ACCL_16_SCALE;
    if (Debug) {
      Serial.println("±16 g");
    }
    break;
  default:
    break;
  }
  values.AcX = (float) data_in.AcX / scale_value;
  values.AcY = (float) data_in.AcY / scale_value;
  values.AcZ = (float) data_in.AcZ / scale_value;

  values.Tmp = (float) data_in.Tmp / 340.0 + 36.53;

  if (Debug) {
    Serial.print(" GyX = ");
    Serial.print(values.GyX);
    Serial.print(" °/s| GyY = ");
    Serial.print(values.GyY);
    Serial.print(" °/s| GyZ = ");
    Serial.print(values.GyZ);
    Serial.print(" °/s| Tmp = ");
    Serial.print(values.Tmp);
    Serial.print(" °C| AcX = ");
    Serial.print(values.AcX);
    Serial.print(" g| AcY = ");
    Serial.print(values.AcY);
    Serial.print(" g| AcZ = ");
    Serial.print(values.AcZ);
    Serial.println(" g");
  }

  return values;
}
//
rawdata offsets;

void calibrateMPU6050(byte addr, rawdata & offsets, char up_axis, int num_samples, bool Debug);
rawdata averageSamples(rawdata * samps, int len);

rawdata mpu6050Read(byte addr, bool Debug) {
  // This function reads the raw 16-bit data values from
  // the MPU-6050

  rawdata values;

  Wire.beginTransmission(addr);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(addr, 14, true); // request a total of 14 registers
  values.AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  values.AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  values.AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  values.Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  values.GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  values.GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  values.GyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  values.AcX -= offsets.AcX;
  values.AcY -= offsets.AcY;
  values.AcZ -= offsets.AcZ;
  values.GyX -= offsets.GyX;
  values.GyY -= offsets.GyY;
  values.GyZ -= offsets.GyZ;

  if (Debug) {
    Serial.print(" GyX = ");
    Serial.print(values.GyX);
    Serial.print(" | GyY = ");
    Serial.print(values.GyY);
    Serial.print(" | GyZ = ");
    Serial.print(values.GyZ);
    Serial.print(" | Tmp = ");
    Serial.print(values.Tmp);
    Serial.print(" | AcX = ");
    Serial.print(values.AcX);
    Serial.print(" | AcY = ");
    Serial.print(values.AcY);
    Serial.print(" | AcZ = ");
    Serial.println(values.AcZ);
  }

  return values;
}

void calibrateMPU6050(byte addr, rawdata & offsets, char up_axis, int num_samples, bool Debug) {
  // This function reads in the first num_samples and averages them
  // to determine calibration offsets, which are then used in
  // when the sensor data is read.

  // It simply assumes that the up_axis is vertical and that the sensor is not
  // moving.
  rawdata temp[num_samples];
  int scale_value;
  byte Gyro, Accl;

  for (int i = 0; i < num_samples; i++) {
    temp[i] = mpu6050Read(addr, false);
  }

  offsets = averageSamples(temp, num_samples);
  getMPU6050scales(MPU_addr, Gyro, Accl);

  switch (Accl) {
  case 0:
    scale_value = (int) MPU_ACCL_2_SCALE;
    break;
  case 1:
    scale_value = (int) MPU_ACCL_4_SCALE;
    break;
  case 2:
    scale_value = (int) MPU_ACCL_8_SCALE;
    break;
  case 3:
    scale_value = (int) MPU_ACCL_16_SCALE;
    break;
  default:
    break;
  }

  switch (up_axis) {
  case 'X':
    offsets.AcX -= scale_value;
    break;
  case 'Y':
    offsets.AcY -= scale_value;
    break;
  case 'Z':
    offsets.AcZ -= scale_value;
    break;
  default:
    break;
  }
  if (Debug) {
    Serial.print(" Offsets: GyX = ");
    Serial.print(offsets.GyX);
    Serial.print(" | GyY = ");
    Serial.print(offsets.GyY);
    Serial.print(" | GyZ = ");
    Serial.print(offsets.GyZ);
    Serial.print(" | AcX = ");
    Serial.print(offsets.AcX);
    Serial.print(" | AcY = ");
    Serial.print(offsets.AcY);
    Serial.print(" | AcZ = ");
    Serial.println(offsets.AcZ);
  }
}

rawdata averageSamples(rawdata * samps, int len) {
  rawdata out_data;
  scaleddata temp;

  temp.GyX = 0.0;
  temp.GyY = 0.0;
  temp.GyZ = 0.0;
  temp.AcX = 0.0;
  temp.AcY = 0.0;
  temp.AcZ = 0.0;

  for (int i = 0; i < len; i++) {
    temp.GyX += (float) samps[i].GyX;
    temp.GyY += (float) samps[i].GyY;
    temp.GyZ += (float) samps[i].GyZ;
    temp.AcX += (float) samps[i].AcX;
    temp.AcY += (float) samps[i].AcY;
    temp.AcZ += (float) samps[i].AcZ;
  }

  out_data.GyX = (int16_t)(temp.GyX / (float) len);
  out_data.GyY = (int16_t)(temp.GyY / (float) len);
  out_data.GyZ = (int16_t)(temp.GyZ / (float) len);
  out_data.AcX = (int16_t)(temp.AcX / (float) len);
  out_data.AcY = (int16_t)(temp.AcY / (float) len);
  out_data.AcZ = (int16_t)(temp.AcZ / (float) len);

  return out_data;

}

// Gyro End


