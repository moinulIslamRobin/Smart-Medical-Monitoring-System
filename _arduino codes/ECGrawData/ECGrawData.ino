#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>
//==============================================================================================
int timezone = 6 * 3600;
int dst = 0;
//==============================================================================================
long tiempo_prev;
float dt;
//==============================================================================================
void setup() {
//============================================================================================
  // initialize the serial communication:
  Serial.begin(9600);
  pinMode(D1, INPUT); // Setup for leads off detection LO +
  pinMode(D2, INPUT); // Setup for leads off detection LO -
//============================================================================================
  WiFi.begin("AndroidAP safat", "trexis-awesome");   //WiFi connection
 
  while (WiFi.status() != WL_CONNECTED) {  //Wait for the WiFI connection completion
 
    delay(500);
    Serial.println("Waiting for connection"); 
  }
  Serial.println("WiFi Connected!");
//=============================================================================================
  configTime(timezone, dst, "pool.ntp.org","time.nist.gov");
}
//============================================================================================= 
void loop() {
  Serial.println("!!Starting!!");
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  String day = String(p_tm->tm_mday);
  String mon = String(p_tm->tm_mon+1);
  String year = String(p_tm->tm_year + 1900);
  String hour = String(p_tm->tm_hour);
  String mint = String(p_tm->tm_min);
  String sec = String(p_tm->tm_sec);
  String curTime = hour+":"+mint+":"+sec;
  String daymon = day+"/"+mon+"/"+year+"  "+curTime;
  Serial.print(daymon);
  //===========================================================================================
  if((digitalRead(10) == 1)||(digitalRead(11) == 1)){
    Serial.println('!');
  }
  else{
    // send the value of analog input 0:      
      Serial.println(analogRead(A0));
  }
  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status      
      StaticJsonBuffer<300> JSONbuffer;   //Declaring static JSON buffer
      JsonObject& JSONencoder = JSONbuffer.createObject(); 
   
      JSONencoder["Device"] = "ECG";
   
      JsonArray& values = JSONencoder.createNestedArray("Value"); //JSON array
      values.add(analogRead(A0)); //Add value to array
      values.add(daymon); //Add Date and Time to array
      //==========================================================================================
      char JSONmessageBuffer[300];
      JSONencoder.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
      Serial.println(JSONmessageBuffer);
   
      HTTPClient http;    //Declare object of class HTTPClient
   
      http.begin("http://104.197.51.157/smart_health/");      //Specify request destination
      http.addHeader("Content-Type", "application/json");  //Specify content-type header
   
      int httpCode = http.POST(JSONmessageBuffer);   //Send the request
      String payload = http.getString();                                        //Get the response payload
   
      Serial.println(httpCode);   //Print HTTP return code
      Serial.println(payload);    //Print request response payload
   
      http.end();  //Close connection
      
      } else {
   
      Serial.println("Error in WiFi connection");
   
    }

}
