/*Put some commments here*/

#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {}; //todo add mac address
IPAddress ip(192, 168, 0, 78); //todo update ip address
EthernetServer server(80);
const int sspin = 10; //chip select pin

const float scaleFactor = 5.0 / 1024.0;
const float sixtySixC = 3.35;
const int ampPin = 2;
const int doorPin = 3;


void setup() {
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
  char reportText;
  float reportValue;
  
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
    Serial.println("Amp too hot");
  }

  int doorStatus = digitalRead(doorPin);
  if (doorStatus == LOW) {
    Serial.println("Door Open");
  }
  
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    int requestStatus = 0;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);

        //--------------------------- parsing the request ----------------------------------------
        
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
            case 'r': //reset power amp turn off
              digitalWrite(ampPin, HIGH);
              reportText = "Turned on power amp";
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
