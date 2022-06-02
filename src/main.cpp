#include "Arduino.h"
#include "ESPAsyncWebServer.h"
#include "DNSServer.h"

AsyncWebServer server(80);

const int maxRotationFastAdjust = 2000;
const int ADJUSTMENT_NUMBER = 1000;
int rotateSteps = 0;

// HTML web page to handle 3 input fields (input1, input2, input3)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    Minutos: <input type="text" name="input1">
    <input type="submit" value="Ajustar">
</body></html>)rawliteral";

const char* PARAM_INPUT_1 = "input1";

// Please tune the following value if the clock gains or loses.
// Theoretically, standard of this value is 60000.
#define MILLIS_PER_MIN 60000 // milliseconds per a minute

// Motor and clock parameters
// 4096 * 110 / 8 = 56320
#define STEPS_PER_ROTATION 56320 // steps for a full turn of minute rotor

// wait for a single step of stepper
int delaytime = 2;

// ports used to control the stepper motor
// if your motor rotate to the opposite direction, 
// change the order as {7, 6, 5, 4};
int port[4] = {16, 17, 18, 19};

// sequence of stepper motor control
int seq[8][4] = {
  {  LOW, HIGH, HIGH,  LOW},
  {  LOW,  LOW, HIGH,  LOW},
  {  LOW,  LOW, HIGH, HIGH},
  {  LOW,  LOW,  LOW, HIGH},
  { HIGH,  LOW,  LOW, HIGH},
  { HIGH,  LOW,  LOW,  LOW},
  { HIGH, HIGH,  LOW,  LOW},
  {  LOW, HIGH,  LOW,  LOW}
};

void rotate(int step) {
  static int phase = 0;
  int i, j;
  int delta = (step > 0) ? 1 : 7;
  int dt = 20;

  step = (step > 0) ? step : -step;
  for(j = 0; j < step; j++) {
    phase = (phase + delta) % 8;
    for(i = 0; i < 4; i++) {
      digitalWrite(port[i], seq[phase][i]);
    }
    delay(dt);
    if(dt > delaytime) dt--;
  }
  // power cut
  for(i = 0; i < 4; i++) {
    digitalWrite(port[i], LOW);
  }
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void showWebPage() 
{
  WiFi.mode(WIFI_STA);
  WiFi.begin("MEO-A5F920", "4a0d05e902");
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return;
  }
  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());


    // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      rotateSteps = inputMessage.toInt() * ADJUSTMENT_NUMBER;
    }
    
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" 
                                     + inputParam + ") with value: " + inputMessage +
                                     "<br><a href=\"/\">Return to Home Page</a>");
  });
  server.onNotFound(notFound);
  server.begin();
}

void setup() {
  //Serial.begin(115200);

  pinMode(port[0], OUTPUT);
  pinMode(port[1], OUTPUT);
  pinMode(port[2], OUTPUT);
  pinMode(port[3], OUTPUT);
  rotate(-20); // for approach run
  rotate(20); // approach run without heavy load
  rotate(STEPS_PER_ROTATION / 60);
  showWebPage();
}

void rotateOnDemand() {
  if (rotateSteps != 0) {
    Serial.println("Rotate on demand. Minutes: ");
    Serial.println(rotateSteps);
    Serial.println();
    int cw = (rotateSteps > 0) ? 1 : -1; // clock wise
    int resto = rotateSteps / maxRotationFastAdjust;
    for (int i = 0; i < abs(resto); i++) {
      rotate(maxRotationFastAdjust * cw); // direction with cw
      Serial.println("rotating");
      delay(100);
    }
    rotateSteps =0;
  }
}

void loop() {
  static long prev_min = 0, prev_pos = 0;
  long min;
  static long pos;
  rotateOnDemand();
  min = millis() / MILLIS_PER_MIN;
  if(prev_min == min) {
    return;
  }
  prev_min = min;
  pos = (STEPS_PER_ROTATION * min) / 60;
  rotate(-20); // for approach run
  rotate(20); // approach run without heavy load
  rotate(pos - prev_pos);
  prev_pos = pos;
  
}