/*
  Using the SparkFun Cryptographic Co-processor Breakout ATECC508a (Qwiic)
  By: Pete Lewis
  SparkFun Electronics
  Date: August 5th, 2019
  License: This code is public domain but you can buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Please buy a board from SparkFun!
  https://www.sparkfun.com/products/15573

  This example shows how to create a digital signature (ECC type) on 32 bytes of data.
  Note, this requires that your device be configured with SparkFun Standard Configuration settings.
  By default, this example uses the private key securely stored and locked in slot 0.

  Hardware Connections and initial setup:
  Install artemis in boards manager: http://boardsmanager/All#Sparkfun_artemis
  Plug in your controller board (e.g. Artemis Redboard, Nano, ATP) into your computer with USB cable.
  Connect your Cryptographic Co-processor to your controller board via a qwiic cable.
  Select TOOLS>>BOARD>>"SparkFun Redboard Artemis"
  Select TOOLS>>PORT>> "COM 3" (note, yours may be different)
  Click upload, and follow configuration prompt on serial monitor at 115200.

*/

#include <SparkFun_ATECCX08a_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_ATECCX08a
#include <i2c_t3.h>

ATECCX08A atecc;

uint8_t message[16] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

uint8_t server_public_key[64] ={0X5d,0X9c,0Xc4,0Xcc,0Xee,0X18,0Xf0,0X38,0Xaf,0X35,0X28,0Xfa,0Xbb,0Xe8,0Xf2,0Xb1,0Xa7,0Xee,0Xe4,0Xd1,0X43,0X1c,0Xd4,0X4c,0Xa0,0X77,0Xa5,0X16,0Xf7,0X8e,0X35,0X90,0X6e,0Xb4,0Xc6,0X9e,0X3c,0X06,0Xb3,0Xbd,0Xc6,0Xa4,0X6a,0X2d,0Xea,0X32,0Xa2,0X3b,0X75,0Xb1,0Xe7,0X6a,0X75,0Xc7,0X9f,0X2b,0X80,0X19,0Xb7,0Xb9,0Xfe,0X38,0Xbd,0Xba};


void setup() {
  while(!Serial);
  Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, 100000);
  Serial.begin(115200);
  if (atecc.begin() == true)
  {
    Serial.println("Successful wakeUp(). I2C connections are good.");
  }
  else
  {
    Serial.println("Device not found. Check wiring.");
    while (1); // stall out forever
  }

  printInfo(); // see function below for library calls and data handling

  // check for configuration
  if (!(atecc.configLockStatus))
  {
    Serial.print("Device not configured. Please use the configuration sketch.");
    while (1); // stall out forever.
  }

    Serial.print("Load Server Public Key: \t");
    if (atecc.loadPublicKey(server_public_key) == true) Serial.println("Success!");
    else Serial.println("Failure or Data has been locked.");

    Serial.print("Lock Data-OTP: \t");
    if (atecc.lockDataAndOTP() == true) Serial.println("Success!");
    else Serial.println("Failure or Data has been locked.");

  Serial.println();
  //Read stored public key for ECDH
  atecc.readPublicKey(true);
  Serial.println();
  //Calculate the share secret and load in tempkey!
  atecc.ECDH(atecc.storedPublicKey, ECDH_OUTPUT_IN_TEMPKEY ,0x0000, false);

  //Encrypt data
  atecc.AES_ECB_encrypt(message,0xFFFF,true);
}

void loop()
{
  // do nothing.
}



void printInfo()
{
  // Read all 128 bytes of Configuration Zone
  // These will be stored in an array within the instance named: atecc.configZone[128]
  atecc.readConfigZone(false); // Debug argument false (OFF)

  // Print useful information from configuration zone data
  Serial.println();

  Serial.print("Serial Number: \t");
  for (int i = 0 ; i < 9 ; i++)
  {
    if ((atecc.serialNumber[i] >> 4) == 0) Serial.print("0"); // print preceeding high nibble if it's zero
    Serial.print(atecc.serialNumber[i], HEX);
  }
  Serial.println();

  Serial.print("Rev Number: \t");
  for (int i = 0 ; i < 4 ; i++)
  {
    if ((atecc.revisionNumber[i] >> 4) == 0) Serial.print("0"); // print preceeding high nibble if it's zero
    Serial.print(atecc.revisionNumber[i], HEX);
  }
  Serial.println();

  Serial.print("Config Zone: \t");
  if (atecc.configLockStatus) Serial.println("Locked");
  else Serial.println("NOT Locked");

  Serial.print("Data/OTP Zone: \t");
  if (atecc.dataOTPLockStatus) Serial.println("Locked");
  else Serial.println("NOT Locked");

  Serial.print("Data Slot 0: \t");
  if (atecc.slot0LockStatus) Serial.println("Locked");
  else Serial.println("NOT Locked");

  Serial.println();

  // if everything is locked up, then configuration is complete, so let's print the public key
  if (atecc.configLockStatus) 
  {
    if(atecc.generatePublicKey(0x0000) == false)
    {
      Serial.println("Failure to generate this device's Public Key");
      Serial.println();
    }
  }
}
