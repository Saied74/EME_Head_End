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
byte mac[] = {}; //TODO MAC address from the Arduino ETH shield sticker
IPAddress ip(192, 168, 0, 78); //somewhat arbitrary private MAC accress
EthernetServer server(80); //port 80 is the default HTTP port
const int sspin = 10; //D10 pin is the default Ethernet shield chip select pin


const float scaleFactor = 5.0 / 1024.0;
const float sixtySixC = 3.35;
const int ampPin = 2; //amplifier on - off control TODO: determine the level
const int doorPin = 3; //door open/close sensor LOW = open


void setup() {
  /* During set up, the condition of the serial port is not checked.  That enables attaching
  a terminal to the Arduino for debugging and also continue to be able to operate without a
  terminal attached.  The same is true of Ethernet hardware and cable.  It is checked, but it
  does not stop operatoin.  The fault condition is reported to the terminal (which may or may not
  be present.
  */
  
  //standard serial and Ethernet start up procedure
  Serial.begin(9600);
  Ethernet.init(sspin);
  Ethernet.begin(mac, ip);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }
  // start the server
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  pinMode(ampPin, OUTPUT); //Amp enable
  pinMode(doorPin, INPUT);  //Door sensor
}

void loop() {

  char reportText; //text to be sent to the host based on the type of request
  float reportValue; //value to be sent to the host based on the type of request
  
  int ampSensor = analogRead(A3);
  int sinkSensor = analogRead(A4);
  int airSensor = analogRead(A5);

  float ampPower = ampSensor * scaleFactor;
  float sinkTemp = sinkSensor * scaleFactor;
  float airTemp = airSensor * scaleFactor;

  Serial.println();
  Serial.print("Amp Power: ");
  Serial.println(ampPower);
  Serial.print("Heatsink Temperature: ");
  Serial.println(sinkTemp);
  Serial.print("Air Temperature: ");
  Serial.println(airTemp);

  if (sinkTemp > sixtySixC) {
    digitalWrite(ampPin, LOW);
    Serial.print("Amp too hot.  It is: ");
    Serial.print(sinkTemp);
    Serial.println(" Deg C");
  }

  int doorStatus = digitalRead(doorPin);
  if (doorStatus == LOW) {
    Serial.println("Door Open");
  }

  /*The head end is polled by the base station on a regular basis.  When polled by the base
  station (roughly every 2 seconds as of now), it provides the requested data to the base
  station.  It is envisioned that the head end querries all the data, one at a time, in each
  interval and displays them.  The base station can also command the head end to turn the
  amplifier on and off*/
  
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    /*The queries and commands from the base have the form of ?q=x where ? signifies the start
    of a query string, q signfiers that there is a query (or command) will follow the ?.  The 
    queries and commands are:
    a: provide air temperature
    h: provide heatsink temperature
    p: provide power level
    t: turn on power amp
    f: turn off power amp
    */
    /*request status keeps track of where the scanner is in the command sequence
    0: idle state
    1: ? detected
    2: q detected
    3: = detected - look for the command
    4: command is obtained
    */
    int requestStatus = 0;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);

        //--------------------------- parsing the request ----------------------------------------
        //this comes first so when detecting = increments statusRequest to 3, the next charachter
        //is most likly the command or query charachter.
        
        if (requestStatus == 3) {
          switch (c) {
            case 'a': //requesting air temperature
              reportText = "Air temperature is: ";
              reportValue = airTemp;
              requestStatus++;
            case 'h': //requesting heatsink temperature
              reportText = "Heatsink temperature is: ";
              reportValue = sinkTemp;
              requestStatus++;
            case 'p': //requesting power level
              reportText = "Power output is: ";
              reportValue = ampPower;
               requestStatus++;
            case 'd': //requesting door status
              reportText = "Door is closed";
              reportValue = 0.0;
              if (doorStatus == LOW) {
                reportText = "Door is open";
              }
              requestStatus++;
            case 't': //turn on power amp
              digitalWrite(ampPin, HIGH);
              reportText = "Turned on power amp";
              reportValue = 0.0;
              requestStatus++;
            case 'f':
              digitalWrite(ampPin, LOW);
              reportText = "Turned Off power amp";
              reportValue = 0.0;
              requestStatus++;
            default:
              reportText = "Bad request";
              reportValue = 0.0;
              requestStatus++;
          }
        }
        if (c == '?') {
          requestStatus = 1;
        }
        if (c == "q" && requestStatus == 1) {
          requestStatus++;
        }
        if (c == "=" && requestStatus == 2) {
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
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output reportText and reportValue
          
          client.print(reportText);
          client.println(reportValue);
          client.println("<br />");
          
          client.println("</html>");
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
    Serial.println("client disconnected");
  }
  
  delay(2000);
}
