//Code by Lennart O.

//include libraries

#include <BlynkSimpleEsp32_BLE.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <ESP32CAN.h>
#include <CAN_config.h>
#include <Arduino.h>

//define constants and variables

word outputvoltage = 1162; //variable for charger voltage setpoint, set startup voltage to 116,2V (offset = 0,1)
word outputcurrent = 100; //variable for charger current setpoint, set startup current to 10A (offset = 0,1)

float pv_voltage;
float pv_current;

int charger_enable = LOW;
String charger_status = "Laden aus";

CAN_frame_t rx_frame;
CAN_frame_t tx_frame;

#define BLYNK_PRINT Serial
#define BLYNK_USE_DIRECT_CONNECT

#define SWITCH  1  // V1, Charger on/off switch
#define V_SLIDER  2 // V2, voltage setting slider
#define C_SLIDER  3 // V3, current setting slider
#define V_DISPLAY 4 // V4, present charge voltage display
#define C_DISPLAY 5 // V5, present charge current display
#define S_DISPLAY 6 // V6, present charger status display

char auth[] = ""; //put your Blynk auth key in here

//define objects

CAN_device_t CAN_cfg; //can bus object
//BlynkTimer timer; //timer object
unsigned long previousMillis1;
unsigned long previousMillis2;
unsigned long currentMillies;

/************************************************
** Function name:           setVoltage
** Descriptions:            set target voltage
*************************************************/

void setVoltage(int t_voltage) { //can be used to set desired voltage to i.e. 80% SOC

  if(t_voltage >= 980.0 && t_voltage <= 1162.0){ //makes sure voltage is in range of Zero

    outputvoltage = t_voltage;

  }

 }

/************************************************
** Function name:           setCurrent
** Descriptions:            set target current
*************************************************/

void setCurrent(int t_current) { //can be used to reduce or adjust charging speed

  if(t_current >= 0.0 && t_current <= 320.0){ //makes sure current is in range of Charger

    outputcurrent = t_current;

  }

}

/************************************************
** Function name:           BLYNK_READ
** Descriptions:            send present charge voltage to app
*************************************************/

BLYNK_READ(V_DISPLAY){ //Blynk app has something on V5

  Blynk.virtualWrite(V_DISPLAY, pv_voltage); //sending to Blynk

}

/************************************************
** Function name:           BLYNK_READ
** Descriptions:            send present charge current to app
*************************************************/

BLYNK_READ(C_DISPLAY){ //Blynk app has something on V5

  Blynk.virtualWrite(C_DISPLAY, pv_current); //sending to Blynk

}

/************************************************
** Function name:           BLYNK_READ
** Descriptions:            send present charger status to app
*************************************************/

BLYNK_READ(S_DISPLAY){ //Blynk app has something on V5

  Blynk.virtualWrite(S_DISPLAY, charger_status); //sending to Blynk

}

/************************************************
** Function name:           BLYNK_WRITE
** Descriptions:            receive switch signal
*************************************************/

BLYNK_WRITE(SWITCH){ //Switch is writing to V1

  int on_off = param.asInt(); // assigning incoming value from pin V1 to a variable

  if(charger_enable == LOW && on_off == HIGH){

    charger_enable = HIGH;
    charger_status = "Laden ein";

  }else if(charger_enable == HIGH && on_off == LOW){

    charger_enable = LOW;
    charger_status = "Laden aus";

  }

}

/************************************************
** Function name:           BLYNK_WRITE
** Descriptions:            receive voltage slider signal
*************************************************/

BLYNK_WRITE(V_SLIDER){ //Voltage slider is writing to V2

  float slider_voltage = param.asFloat(); // assigning incoming value from pin V1 to a variable

  slider_voltage = (int) (slider_voltage*10); //charger protocol offset

  if(slider_voltage > 1162){

    setVoltage(1162);

  }else{

    setVoltage(slider_voltage); //set desired voltage

  }

  //serial.println(outputvoltage); //print out set value for diagnose

}

/************************************************
** Function name:           BLYNK_WRITE
** Descriptions:            receive current slider signal
*************************************************/

BLYNK_WRITE(C_SLIDER){ //Current slider is writing to V3

  float slider_current = param.asFloat(); // assigning incoming value from pin V1 to a variable

  slider_current = (int) (slider_current*10); //charger protocol offset

  if(slider_current > 320){

    setCurrent(320);

  }else{

    setCurrent(slider_current); //set desired voltage

  }

  //serial.println(outputcurrent); //print out set value for diagnose

}

/************************************************
** Function name:           canRead
** Descriptions:            read CAN message
*************************************************/

void canRead(){

  if(xQueueReceive(CAN_cfg.rx_queue,&rx_frame, 3*portTICK_PERIOD_MS)==pdTRUE){

    /*if(rx_frame.FIR.B.FF==CAN_frame_std){

      //serial.println("New standard frame");
      //serial.println();

    }else{

      //serial.println("New extended frame");
      //serial.println();

    }*/

    /*if(rx_frame.FIR.B.RTR==CAN_RTR)

      Serial.printf(" Message from ID 0x%08x, Length: %d\r\n",rx_frame.MsgID,  rx_frame.FIR.B.DLC);

    else{

      //Serial.printf("Message from ID 0x%08x, Length: %d\n",rx_frame.MsgID,  rx_frame.FIR.B.DLC);

      for(int i = 0; i < 8; i++){

        if(rx_frame.data.u8[i] < 0x10){ // führende Null wenn nur eine Ziffer

          //serial.print("0");

        }

        //serial.print(rx_frame.data.u8[i], HEX);
        //serial.print(" ");

      }*/

      //serial.println(); //Absatz

      //serial.print("Ladespannung: ");
      pv_voltage = (((float)rx_frame.data.u8[0]*256.0) + ((float)rx_frame.data.u8[1]))/10.0; //highByte/lowByte + offset
      //serial.print(pv_voltage);
      //serial.print(" V / Ladestrom: ");
      pv_current = (((float)rx_frame.data.u8[2]*256.0) + ((float)rx_frame.data.u8[3]))/10.0; //highByte/lowByte + offset
      //serial.print(pv_current);
      //serial.println(" A"); //Absatz

      switch (rx_frame.data.u8[4]) { //Fehlerbyte auslesen

        case B00000001: charger_status = "Hardwarefehler";break;
        case B00000010: charger_status = "Fehler: Übertemperatur";break;
        case B00000100: charger_status = "Fehler: Eingangsspannung unzulässig";break;
        case B00001000: charger_status = "Fehler: Batterie nicht verbunden";break;
        case B00010000: charger_status = "Fehler: CAN-Bus Fehler";break;
        case B00001100: charger_status = "Fehler: Keine Eingangsspannung";break;

      //}

      //serial.println();
      //Serial.println();

    }

  }

}

/************************************************
** Function name:           canWrite
** Descriptions:            write CAN message
*************************************************/

void canWrite(){

  unsigned char voltamp[8] = {highByte(outputvoltage), lowByte(outputvoltage), highByte(outputcurrent), lowByte(outputcurrent), !charger_enable,0x00,0x00,0x00};

  tx_frame.FIR.B.FF = CAN_frame_ext;
  tx_frame.MsgID = 0x1806E5F4;
  tx_frame.FIR.B.DLC = 8;

  for(int i = 0; i < 8; i++){

    tx_frame.data.u8[i] = voltamp[i];

  }

  ESP32Can.CANWriteFrame(&tx_frame);
  //serial.println("Nachricht gesendet");
  //serial.println();

}

/************************************************
** Function name:           myTimer1
** Descriptions:            Function of timer1
*************************************************/

/*void myTimer1() { //zyklisch vom Timer aufgerufene Funktion

  canWrite(); //Schreibfunktion aufrufen
  //serial.println(); //Absatz

}*/

/************************************************
** Function name:           myTimer2
** Descriptions:            Function of timer2
*************************************************/

/*void myTimer2() { //zyklisch vom Timer aufgerufene Funktion

  canRead(); //Lesefunktion aufrufen
  //serial.println(); //Absatz

}*/

/************************************************
** Function name:           setup
** Descriptions:            Arduino setup
*************************************************/

void setup(){

  // Debug console
  //Serial.begin(115200);

  //serial.println("Waiting for connections...");

  CAN_cfg.speed=CAN_SPEED_250KBPS;
  CAN_cfg.tx_pin_id = GPIO_NUM_5;
  CAN_cfg.rx_pin_id = GPIO_NUM_4;
  CAN_cfg.rx_queue = xQueueCreate(1,sizeof(CAN_frame_t));

  ESP32Can.CANInit(); //start CAN Module

  previousMillis1 = millis();
  previousMillis2 = millis();

  //timer.setInterval(951, myTimer1); //send every 951 ms
  //timer.setInterval(970, myTimer2); //read every 970 ms
  //timer.setInterval(1988, myTimer2); //read only every second message of the charger, reduce controller workload

  Blynk.setDeviceName("SpeedCharger");

  Blynk.begin(auth);

}

/************************************************
** Function name:           loop
** Descriptions:            Arduino loop
*************************************************/

void loop(){

  Blynk.run();

  currentMillies = millis();

  if((currentMillies-previousMillis1) >= 975){

    canWrite();
    previousMillis1 = millis();
    previousMillis2 = millis();

  }

  if((currentMillies-previousMillis2) >= 500){

    canRead();
    previousMillis2 = millis();

  }
  //timer.run();

}
