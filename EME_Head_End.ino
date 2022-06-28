/*EME Head End is designed to run on an Arduino Nano and work with a MKR ETH Shield
Its functions are:
1. Measure and report the temperature of the air in the cabinet
2. Measure and report amplifier heatsink temperature
3. Shut down the amplifier when temperature exceeds 66 deg C
4. Report the amplifer shut down condition
5. Reset the shut down condition
6. Measure and report amplifier power output
7. Monitor cabinet door status and report it
*/

#include <SPI.h>
#include <Ethernet.h>

//Static IP address will be used to keep the code size small
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; //TODO MAC address from the Arduino ETH shield sticker
IPAddress ip(192, 168, 0, 20); //somewhat arbitrary private IP accress
EthernetServer server(80); //port 80 is the default HTTP port
const int sspin = 10; //D10 pin is the default Ethernet shield chip select pin
const int sdpin = 9; //D9 pin is chip select for SD Card


const float powerFactor = (4.56 / 1024.0)*(250.0/5.0); //for converting A/D reads into power
const float tempFactor = (4.56 / 1024) * 100.0; //for converting A/D reads to deg K
const float ampThreshold = 66.0; //votage value for 66 degrees C temperature
const int ampPin = 2; //amplifier on - off control TODO: determine the level
const int doorPin = 3; //door open/close sensor LOW = open

String ampStatus = "Off";


void setup() {
  /* During set up, the condition of the serial port is not checked.  That enables attaching
  a terminal to the Arduino for debugging and also continue to be able to operate without a
  terminal attached.  The same is true of Ethernet hardware and cable.  It is checked, but it
  does not stop operatoin.  The fault condition is reported to the terminal (which may or may not
  be present).  This will allow debugging if needed.
  */
  
  //standard serial and Ethernet start up procedure
  pinMode(sdpin, OUTPUT);
  digitalWrite(sdpin, HIGH);
  Serial.begin(9600);

  Ethernet.init(sspin);
  SPI.begin();
  Ethernet.begin(mac, ip);

  // Check for Ethernet hardware present
  
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }
  }

  // start the server
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  pinMode(ampPin, OUTPUT); //Amp enable
  pinMode(doorPin, INPUT);  //Door sensor
 
  
  
}

void loop() {

  String reportText; //text to be sent to the host based on the type of request
  String reportValue; //value, if any, to be sent to the host based on the type of request
  
  int ampSensor = analogRead(A3);
  int sinkSensor = analogRead(A6);
  int airSensor = analogRead(A5);

  float ampPower = ampSensor * powerFactor;
  float sinkTemp = sinkSensor * tempFactor - 273.15;
  float airTemp = airSensor * tempFactor - 273.15;

  int doorStatus = digitalRead(doorPin);
  

  /*The head end is polled by the base station on a regular basis.  When polled by the base
  station (roughly every 2 seconds as of now), it provides the requested data to the base
  station.  It is envisioned that the head end querries all the data, one at a time, in each
  interval and displays them.  The base station can also command the head end to turn the
  amplifier on and off*/
  /*request status keeps track of where the scanner is in the command sequence
    0: idle state
    1: ? detected
    2: q detected
    3: = detected - look for the command
    4: command is obtained
    */
   int requestStatus = 0;
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    //Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        //Serial.print(c);

        //--------------------------- parsing the request ----------------------------------------
        //this comes first so when detecting = increments statusRequest to 3, the next charachter
        //is most likly the command or query charachter.
        
        if (requestStatus == 3) {
          switch (c) {
            case 'r': //report status
              reportText = "airTemp=";
              reportValue = String(airTemp, 1);
              reportText += reportValue;
              reportText += ";";
              reportText+= "sinkTemp=";
              reportValue = String(sinkTemp, 1);
              reportText+= reportValue;
              reportText+= ";";
              reportText+= "ampPower=";
              reportValue = String(ampPower, 1);
              reportText+= reportValue;
              reportText+= ";";
              reportText+= "doorStatus=";
              if (doorStatus == LOW){
                reportText+= "Open";
              } else {
                reportText+= "Closed";
              }
              reportText+= ";";
              reportText+= "ampStatus=";
              reportText+= ampStatus;
              requestStatus++;
              break;
            case 't': //turn on power amp
              digitalWrite(ampPin, HIGH);
              ampStatus = "On";
              reportText = "nothing=nothing";//to pacify the error handler on the other side
              requestStatus++;
               break;
            case 'f':
              digitalWrite(ampPin, LOW);
              ampStatus = "Off";
              reportText = "nothing=nothing"; //to pacify the error handler on the other side
              requestStatus++;
               break;
            default:
              reportText = "badReq";
              requestStatus++;
          }
        }
        if (c == '?') {
          requestStatus = 1;
        }
        if (c == 'q' && requestStatus == 1) {
          requestStatus++;
        }
        if (c == '=' && requestStatus == 2) {
          requestStatus++;
        }

        //----------------------------------------------------------------------------------------
        /*TODO:  This section may have to be changed to report the results in the URL line to
        make parsing it easier.  Since the connection is private using Ethernet over coaxial cable
        between the headend and the base station, safety of the request is not an issue:
       */
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/plain");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println(reportText);
          //break out of the loop when done
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    //Serial.println("client disconnected");
  }
  
  delay(100);
}
