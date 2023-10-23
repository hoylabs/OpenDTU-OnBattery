#include "SMA_HM.h"
#include <Arduino.h>
#include "Configuration.h"
#include "NetworkSettings.h"
#include <WiFiUdp.h>
#include "MessageOutput.h"

unsigned int multicastPort = 9522;  // local port to listen on
IPAddress multicastIP(239, 12, 255, 254);
WiFiUDP SMAUdp;

const long interval = 5000; 

static void Soutput(int kanal, int index, int art, int tarif, String Bezeichnung, double value, int timestamp){
  MessageOutput.print(Bezeichnung);
  MessageOutput.print('=');
  MessageOutput.println(value);
}

SMA_HMClass SMA_HM;

void SMA_HMClass::init()
{
    //emeter.init();
    SMAUdp.begin(multicastPort);
    SMAUdp.beginMulticast(multicastIP, multicastPort);
}

void SMA_HMClass::loop()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
      event1();
  }
}

void SMA_HMClass::event1() 
{  
    
    //emeter.loop();  
    uint8_t buffer[1024];
    int packetSize = SMAUdp.parsePacket();
    double Pbezug,L1bezug,L2bezug,L3bezug = 0;
    double Peinspeisung,L1einspeisung,L2einspeisung,L3einspeisung = 0;
    double Leistung,L1Leistung,L2Leistung,L3Leistung = 0;
    bool output = false;
    if (packetSize) {
        int rSize = SMAUdp.read(buffer, 1024);
        if (buffer[0] != 'S' || buffer[1] != 'M' || buffer[2] != 'A') {
            MessageOutput.println("Not an SMA packet?");
            return;
        }
        uint16_t grouplen;
        uint16_t grouptag;
        uint8_t* offset = buffer + 4;
        do {
            grouplen = (offset[0] << 8) + offset[1];
            grouptag = (offset[2] << 8) + offset[3];
            offset += 4;
            if (grouplen == 0xffff) return;
            if (grouptag == 0x02A0 && grouplen == 4) {
                offset += 4;
            } else if (grouptag == 0x0010) {
                uint8_t* endOfGroup = offset + grouplen;
                uint16_t protocolID = (offset[0] << 8) + offset[1];
                offset += 2;
                uint16_t susyID = (offset[0] << 8) + offset[1];
                offset += 2;
                uint32_t serial = (offset[0] << 24) + (offset[1] << 16) + (offset[2] << 8) + offset[3];
                SMA_HM.serial=serial;
                offset += 4;
                uint32_t timestamp = (offset[0] << 24) + (offset[1] << 16) + (offset[2] << 8) + offset[3];
                offset += 4;
                while (offset < endOfGroup) {
                    uint8_t kanal = offset[0];
                    uint8_t index = offset[1];
                    uint8_t art = offset[2];
                    uint8_t tarif = offset[3];
                    offset += 4;
                    if (art == 8) {
                        uint64_t data = ((uint64_t)offset[0] << 56) +
                                      ((uint64_t)offset[1] << 48) +
                                      ((uint64_t)offset[2] << 40) +
                                      ((uint64_t)offset[3] << 32) +
                                      ((uint64_t)offset[4] << 24) +
                                      ((uint64_t)offset[5] << 16) +
                                      ((uint64_t)offset[6] << 8) +
                                      offset[7];
                        offset += 8;
                    } else if (art == 4) {
                      uint32_t data = (offset[0] << 24) +
                      (offset[1] << 16) +
                      (offset[2] << 8) +
                      offset[3];
                      offset += 4;
                      switch (index) {
                      case (1):
                        Pbezug = data * 0.1;
                        break;
                      case (2):
                        output = true;
                        Peinspeisung = data * 0.1;
                        break;
                      case (21):
                        L1bezug = data * 0.1;
                        break;
                      case (22):
                        output = true;
                        L1einspeisung = data * 0.1;
                        break;
                      case (41):
                        L2bezug = data * 0.1;
                        break;
                      case (42):
                        output = true;
                        L2einspeisung = data * 0.1;
                        break;
                      case (61):
                        L3bezug = data * 0.1;
                        break;
                      case (62):
                        output = true;
                        L3einspeisung = data * 0.1;
                        break;
                      default:
                        break; // Wird nicht benötigt, wenn Statement(s) vorhanden sind
                      }
                        if (output == true){
                        Leistung = Peinspeisung - Pbezug;
                        L1Leistung=L1einspeisung-L1bezug;
                        L2Leistung=L2einspeisung-L2bezug;
                        L3Leistung=L3einspeisung-L3bezug;
                        _powerMeterPower = Leistung;
                        _powerMeterL1=L1Leistung;
                        _powerMeterL2=L2Leistung;
                        _powerMeterL3=L3Leistung;
                        Soutput(kanal, index, art, tarif, "Leistung", Leistung, timestamp);
                        output = false;
                      }
                    } else if (kanal==144) {
                      // Optional: Versionsnummer auslesen... aber interessiert die?
                      offset += 4;
                    } else {
                        offset += art;
                        MessageOutput.println("Strange measurement skipped");
                    }
                }
            } else if (grouptag == 0) {
                // end marker
                offset += grouplen;
            } else {
                MessageOutput.print("unhandled group ");
                MessageOutput.print(grouptag);
                MessageOutput.print(" with len=");
                MessageOutput.println(grouplen);
                offset += grouplen;
            }
        } while (grouplen > 0 && offset + 4 < buffer + rSize);
    }
}
float SMA_HMClass::getPowerTotal()
{
    return _powerMeterPower;
}
float SMA_HMClass::getPowerL1()
{
    return _powerMeterL1;
}
float SMA_HMClass::getPowerL2()
{
    return _powerMeterL2;
}
float SMA_HMClass::getPowerL2()
{
    return _powerMeterL3;
}