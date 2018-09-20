/* Programm for reading values from magnetic sensor HMC5883
    data sheet HMC5883: https://cdn-shop.adafruit.com/datasheets/HMC5883L_3-Axis_Digital_Compass_IC.pdf
    the use of libraries has been ommitted´by reverse engineering them.

    Gain:
    The gain is set to minimum (4.35 mG per count). (HMC5883 Configuration Register B: 0x01).
    The Byte has to be set to 11100000 = 0xE0. The counting range is -2048 to 2047 resulting
    in a measurement range of -8.9 G to 8.9 G. Default gain is 0.92 mG per count for the
    register value of 00100000 = 0x20.

    Gauging:
    It turned out, that a scaling (gauging) of the axes is necessairy: x, y and z.
    By adjusting the axes parallel or antiparallel to the earth' magnetic field B, after
    giving the gauging command, the maximum and minimum values are determined (xp,xm,yp,ym, zp and zm),
    (p stands for positive (parallel) and m for minus (antiparallel) to earth magnetic field B)
    Gauging can be invoked by commands ( <1> for adjusting xp, <2> for xm and so on).
    Gauging for an axis ends, when another command is given (preferentially 0 or a new to be gauged axis).
    If the same axis is commanded morefold, the gauging starts for this axis again.

    Gauging is supposed to be performed by the magnetic earth field B, which is in Berlin 0.5 G
    (and directed roughly to North and about 70° downwards from horizontal).
    As start values, the ideal behaviour of the sensor is supposed (4.35 mG per count),
    so the readings should be for an axis parallel or antparallel +/- 115 counts = (0.5/0.00435)= cpemf counts.
    The sensor values are measured in counts, called rx. The scaled values are called sx and so on.
    During gauging the respective maximum is set to cpemf = 115 for Berlin and minimum gain.
*/

#include <Wire.h> //I2C Arduino Library


#define HMC5883_ADDRESS_MAG            (0x3C >> 1)
int cpemf = 115; //this means counts per earth magnetic field (nominal)and should be 115 for Berlin (0.5 G)
//and minimum gain (4.35 mG/count), the respective axis has to be parallel or antiparallel to B
int NewCommand = 0;
float ExtrVal = 1.0;
float rx = 0, ry = 0, rz = 0;// raw values directly red from the sensor and avaraged
float sx = 0, sy = 0, sz = 0;// scaled sensor values
float xp = cpemf, xm = cpemf;// scaling constants
float yp = cpemf, ym = cpemf;
float zp = cpemf, zm = cpemf;
long oldtime = 0;

void setup() {
  Serial.begin(9600);

  // Enable I2C
  Wire.begin();

  // Set magnetometer to continuous operation, since default is single measurement
  Wire.beginTransmission(HMC5883_ADDRESS_MAG);
  Wire.write((uint8_t) 0x02); //HMC5883_REGISTER Mode Register
  Wire.write((uint8_t)0x00);
  Wire.endTransmission();
  // Set gain to minimum (4.35 mG per count)
  // So it is attained for Berlin (earth magnetic field = 0.5 G) cpemf counts = 0.5 G/0.00435 G
  Wire.beginTransmission(HMC5883_ADDRESS_MAG);
  Wire.write((uint8_t) 0x01); //HMC5883_REGISTER Configuration Register B
  Wire.write((uint8_t)0xE0);
  Wire.endTransmission();


  oldtime = millis();
}

void loop()
{
  if (Serial.available()) {
    int OldCommand = NewCommand;//save the previous Command value
    NewCommand = (int)(Serial.read() - '0');
    // Set it to zero, when out of range
    if (NewCommand < 0 || NewCommand > 7)
      NewCommand = 0;
    // Call Extremum with NewCommand (when it is 7, resulting in reading data from Serial)
    if (NewCommand == 7)
      Extremum(NewCommand, false);
    // When NewCommand is a change, overtake the old values (stop gauging for the last axis)
    else if (NewCommand != OldCommand)
      Extremum(OldCommand, true);
    ExtrVal = 1.0;//restore the extreme value for the next gauging procedure
    Serial.find('\n'); //find end of message
  }
  // if gauging is in progress, call Extremum again and again
  if (NewCommand)
    Extremum(NewCommand, false);

  Aread();
  SSend ("v:" + (String)sx + ":" + (String)sy + ":" + (String)sz + ":");
}

//------------------------Reading values from the sensor and scale-------------------------
void Aread()
{
  float helperx = 0, helpery = 0, helperz = 0;

  // Read the magnetometer
  Wire.beginTransmission((byte)HMC5883_ADDRESS_MAG);
  Wire.write(0x03); //HMC5883_REGISTER
  Wire.endTransmission();
  // 6 Bytes are requested, the addresses are counted firther automatically
  Wire.requestFrom((byte)HMC5883_ADDRESS_MAG, (byte)6);

  // Wait around until enough data are available
  while (Wire.available() < 6);

  // Note high before low (different than accelerometer)
  uint8_t xhi = Wire.read();
  uint8_t xlo = Wire.read();
  uint8_t zhi = Wire.read();
  uint8_t zlo = Wire.read();
  uint8_t yhi = Wire.read();
  uint8_t ylo = Wire.read();

  // Shift values to create properly formed integer (low byte first)
  helperx = ((int16_t)(xlo | ((int16_t)xhi << 8)));
  helpery = ((int16_t)(ylo | ((int16_t)yhi << 8)));
  helperz = ((int16_t)(zlo | ((int16_t)zhi << 8)));
  //filtering values with gliding avarage
  rx = rx * 0.99 + helperx * 0.01;
  ry = ry * 0.99 + helpery * 0.01;
  rz = rz * 0.99 + helperz * 0.01;
  // correct the values to gauging values xp ... zm, see document Magnetometer Extreme Calibration.doc
  sx = cpemf*(2*(rx + xm)/(xp + xm) - 1);
  sy = cpemf*(2*(ry + ym)/(yp + ym) - 1);
  sz = cpemf*(2*(rz + zm)/(zp + zm) - 1);
}

void Extremum(int Command, bool overtake) {
  float Help; //to check on zero values

  switch (Command) {
    case 1:
      if (overtake) { // take over xp of the previous gauging
        xp = ExtrVal ; 
        Serial.println("xp:" + (String)xp + ":");//finally the xp is printed despite of timing
      }
      else {    // gauge xp
        Aread();
        if (rx > ExtrVal)// take the new raw value, when it exceeds the others
          ExtrVal = rx;
        SSend("xp:" + (String)(ExtrVal) + ":");//during gauging ExtrVal is shown
      }
      break;
    case 2:
      if (overtake){ // take over xm of the previous gauging
        xm = abs(ExtrVal) ;
        Serial.println("xm:" + (String)(xm) + ":");//finally the xm is printed despite of timing
      }
      else {    // gauge xm
        Aread();
        if (rx < ExtrVal)// take the new raw value, when it exceeds the others
          ExtrVal = rx;
        SSend("xm:" + (String)(ExtrVal) + ":");//during gauging ExtrVal is shown
      }
      break;
    case 3:
      if (overtake){ // take over yp of the previous gauging
        yp = ExtrVal ;
        Serial.println("yp:" + (String)(yp) + ":");//finally the yp is printed despite of timing
      }
      else {    // gauge yp
        Aread();
        if (ry > ExtrVal)// take the new raw value, when it exceeds the others
          ExtrVal = ry;
        SSend("yp:" + (String)(ExtrVal) + ":");//during gauging ExtrVal is shown
      }
      break;
    case 4:
      if (overtake){ // take over ym of the previous gauging
        ym = abs(ExtrVal) ;
        Serial.println("ym:" + (String)(ym) + ":");//finally the ym is printed despite of timing
      }
      else {    // gauge ym
        Aread();
        if (ry < ExtrVal)// take the new raw value, when it exceeds the others
          ExtrVal = ry;
        SSend("ym:" + (String)(ExtrVal) + ":");//during gauging ExtrVal is shown
      }
      break;
    case 5:
      if (overtake){ // take over zp of the previous gauging
        zp = ExtrVal ; 
        Serial.println("zp:" + (String)(zp) + ":");//finally the zp is printed despite of timing
      }
      else {    // gauge zp
        Aread();
        if (rz > ExtrVal)// take the new raw value, when it exceeds the others
          ExtrVal = rz;
        SSend("zp:" + (String)(ExtrVal) + ":");//during gauging ExtrVal is shown
      }
      break;
    case 6:
      if (overtake){ // take over zm of the previous gauging
        zm = abs(ExtrVal) ;
        Serial.println("zm:" + (String)(zm) + ":");//finally the zm is printed despite of timing
      }
      else {    // gauge zm
        Aread();
        if (rz < ExtrVal)// take the new raw value, when it exceeds the others
          ExtrVal = rz;
        SSend("zm:" + (String)(ExtrVal) + ":");//during gauging ExtrVal is shown
      }
      break;
    case 7: //Use values from TWedge or Serial Monitor
      Serial.find(':');
      delay(10);
      Help = Serial.readStringUntil(':').toFloat();
      if ( Help != 0)
        xp = (Help);
      delay(10);
      Help = Serial.readStringUntil(':').toFloat();
      if ( Help != 0)
        xm = abs(Help);
      delay(10);
      Help = Serial.readStringUntil(':').toFloat();
      if ( Help != 0)
        yp = (Help);
      delay(10);
      Help = Serial.readStringUntil(':').toFloat();
      if ( Help != 0)
        ym = abs(Help);
      delay(10);
      Help = Serial.readStringUntil(':').toFloat();
      if ( Help != 0)
        zp = (Help);
      delay(10);
      Help = Serial.readStringUntil(':').toFloat();
      if ( Help != 0)
        zm = abs(Help);
      Serial.println ("g:" + (String)(xp) + ":" + (String)(xm) + ":" + (String)(yp) + ":" + (String)(ym) + ":" + (String)(zp) + ":" + (String)(zm) );
      NewCommand = 0;
      break;
    default: break;
  }
}

void SSend (String OutStr) {
  if (millis() - oldtime > 500) {
    Serial.print(OutStr + "\r\n");
    oldtime = millis();
  }
}
