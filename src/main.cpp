#include <Arduino.h>
#include "IRUtils.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <IRsend.h>
#include <ir_Gree.h>
#include <EEPROM.h>
#include <stdint.h>
#include <dht.h>
#include <ESP8266WebServer.h>

// Config
// #Params sent to AC
#define CMD_PARAMS 10
// How many times the command must be sent by IR
// Default 3 times, 1.5secs delay between each repetition
#define CMD_REPEAT 3
// DHT Sensor  on D1 (GPIO5)
#define DHT11_PIN 5
//IR on D2 (GPIO4)
#define IR_PIN 4

// SSID and Password
const char* ssid = "SSID";
const char* password = "PASSWORD";

// IP configuration (default we use DHCP so the following lines are disabled)
//IPAddress ip(xxx, xxx, xxx, xxx);
//IPAddress gateway(xxx, xxx, xxx, xxx);
//IPAddress subnet(xxx, xxx, xxx, xxx);

// WEBUI configuration (CHANGE user&pass! default: antani/antani )
const char *www_username = "antani";
const char *www_password = "antani";
const char *www_realm = "greeAC";
const char *www_error_message = "Uh uh uh! You didn't say the magic word! Uh uh uh! Uh uh uh!";

dht DHT;
IRGreeAC ac(IR_PIN);
ESP8266WebServer server(80);

// AC Remote last command/default command saved into this array
uint8_t state[10];

// Check if state contains allowed values
// If not then replace the content with default values
// [0,24,0,1,1,1,0,0,0,1]
void state_check() {
  //command format:[mode, temp, fan_speed,
  //                flap_auto_flag, flap,
  //                light, turbo, xfan, sleep, on_off]
  uint8_t options=1;
  for(uint8_t j=5;j<CMD_PARAMS;j++) {
    if(state[j]<0 || state[j]>1) {
      options=0;
      break;
    }
  }
  uint8_t temp=1;
  if(state[1]<16 || state[1]>31) {
    temp=0;
  }
  uint8_t flap_flag_auto=1;
  if(state[3]<0 || state[3]>1) {
    flap_flag_auto=0;
  }
  uint8_t flap=1;
  if(state[4]<1 || state[4]>11) {
    flap=0;
  }
  uint8_t fan=1;
  if(state[2]<0 || state[2]>3) {
    fan=0;
  }
  uint8_t mode=1;
  if(state[0]<0 || state[0]>4) {
    mode=0;
  }
  if (!(mode && temp && options && flap_flag_auto && fan && flap)) {
    Serial.println("Incorrect values stored in state[], resetting to default.");
    state[0]=0;
    state[1]=24;
    state[2]=0;
    state[3]=1;
    state[4]=1;
    state[5]=1;
    state[6]=0;
    state[7]=0;
    state[8]=0;
    state[9]=0;
    EEPROM.put(0,state);
    EEPROM.commit();
  }
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

// AC WebGUI (see remote_ac.html)
void handleAC() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication(DIGEST_AUTH,www_realm,www_error_message);
  }

  //Read temp & humidity from DHT11
  delay(500);
  DHT.read11(DHT11_PIN);

  String string_state="[";
  for(int i=0; i<10;i++) {
    char *current=(char *)malloc(sizeof(char)*4);
    snprintf(current, sizeof(char)*3, "%d",state[i]);
    Serial.print("Current i");
    Serial.print(i);
    Serial.print(" val: ");
    Serial.print(current);
    Serial.print(" state orig: ");
    Serial.println(state[i]);
    string_state+=current;
    if(i<9) {
      string_state+=",";
    }
    free(current);
  }
  string_state+="]";
  String webpage="";
  webpage+="<!DOCTYPE html>\n";
  webpage+="<meta charset=\"utf-8\">\n";
  webpage+="<html>\n";
  webpage+="<body onload=\"readstate()\">\n";
  webpage+="\n";
  webpage+="  <style>\n";
  webpage+="  .alert {\n";
  webpage+="    padding: 10px;\n";
  webpage+="    background-color: gray;\n";
  webpage+="    color: white;\n";
  webpage+="    font-family: Arial;\n";
  webpage+="    font-size: 18px;\n";
  webpage+="    width: 260px;\n";
  webpage+="    visibility: hidden;\n";
  webpage+="    border-radius: 10px;\n";
  webpage+="  }\n";
  webpage+="\n";
  webpage+="  </style>\n";
  webpage+="  <style>\n";
  webpage+="  .slidecontainer {\n";
  webpage+="      width: 100%;\n";
  webpage+="      font-size: 24px;\n";
  webpage+="  }\n";
  webpage+="\n";
  webpage+="  .slider {\n";
  webpage+="      -webkit-appearance: none;\n";
  webpage+="      width: 270px;\n";
  webpage+="      height: 25px;\n";
  webpage+="      background: linear-gradient(to right, LightSkyBlue, OrangeRed);\n";
  webpage+="      outline: none;\n";
  webpage+="      opacity: 1.0;\n";
  webpage+="      -webkit-transition: .2s;\n";
  webpage+="      transition: opacity .2s;\n";
  webpage+="      border-radius: 10px;\n";
  webpage+="      border: 0.2px;\n";
  webpage+="\n";
  webpage+="  }\n";
  webpage+="\n";
  webpage+="  .slider:hover {\n";
  webpage+="      opacity: 1;\n";
  webpage+="  }\n";
  webpage+="\n";
  webpage+="  .slider::-webkit-slider-thumb {\n";
  webpage+="      -webkit-appearance: none;\n";
  webpage+="      appearance: none;\n";
  webpage+="      width: 25px;\n";
  webpage+="      height: 25px;\n";
  webpage+="      background: #4CAF50;\n";
  webpage+="      cursor: pointer;\n";
  webpage+="      border-radius: 10px;\n";
  webpage+="  }\n";
  webpage+="\n";
  webpage+="  .slider::-moz-range-thumb {\n";
  webpage+="      width: 25px;\n";
  webpage+="      height: 25px;\n";
  webpage+="      background: #4CAF50;\n";
  webpage+="      cursor: pointer;\n";
  webpage+="  }\n";
  webpage+="  </style>\n";
  webpage+="  <style>\n";
  webpage+="    .btn {\n";
  webpage+="      -webkit-border-radius: 28;\n";
  webpage+="      -moz-border-radius: 28;\n";
  webpage+="      border-radius: 28px;\n";
  webpage+="      font-family: Arial;\n";
  webpage+="      color: #ffffff;\n";
  webpage+="      font-size: 20px;\n";
  webpage+="      padding: 10px 20px 10px 20px;\n";
  webpage+="      text-decoration: none;\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="\n";
  webpage+="    .btn_red {\n";
  webpage+="        background: red;\n";
  webpage+="    }\n";
  webpage+="    .btn_blue {\n";
  webpage+="        background: blue;\n";
  webpage+="    }\n";
  webpage+="  </style>\n";
  webpage+="  <style>\n";
  webpage+="    .box {\n";
  webpage+="      border-radius: 40px;\n";
  webpage+="      padding: 0px;\n";
  webpage+="      //display: inline-block;\n";
  webpage+="      font-family: Arial;\n";
  webpage+="      font-size: 18px;\n";
  webpage+="      vertical-align: middle;\n";
  webpage+="      width: 300px\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .box_blue {\n";
  webpage+="      background: skyblue;\n";
  webpage+="      border: 1px solid blue;\n";
  webpage+="\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .box_green {\n";
  webpage+="      background: lightgreen;\n";
  webpage+="      border: 1px solid green;\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .box_violet {\n";
  webpage+="      background: palevioletred;\n";
  webpage+="      border: 1px solid darkviolet;\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .box_gray {\n";
  webpage+="      background: LightGray;\n";
  webpage+="      border: 1px solid LightSlateGray;\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .remote_box {\n";
  webpage+="      border-radius: 15px;\n";
  webpage+="      border: 2px solid blue;\n";
  webpage+="      background: beige;\n";
  webpage+="      padding: 10px;\n";
  webpage+="      display: inline-block;\n";
  webpage+="      font-family: Arial;\n";
  webpage+="      font-size: 18px;\n";
  webpage+="\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .radio-btn input[type=\"radio\"]  {\n";
  webpage+="      display: none;\n";
  webpage+="\n";
  webpage+="\n";
  webpage+="    }\n";
  webpage+="    .radio-btn label {\n";
  webpage+="      color: #fff;\n";
  webpage+="      font-family: Arial;\n";
  webpage+="      font-size: 16px;\n";
  webpage+="      cursor: pointer;\n";
  webpage+="      padding: 4px;\n";
  webpage+="      border-radius: 15px;\n";
  webpage+="\n";
  webpage+="\n";
  webpage+="\n";
  webpage+="    }\n";
  webpage+="    .radio-btn_blue input[type=\"radio\"]:checked+label {\n";
  webpage+="      background-color: #3279ff;\n";
  webpage+="    }\n";
  webpage+="    .radio-btn_blue label {\n";
  webpage+="      background-color: skyblue;\n";
  webpage+="      border: 0.5px solid #3279ff;\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .radio-btn_green input[type=\"radio\"]:checked+label {\n";
  webpage+="      background-color: green;\n";
  webpage+="    }\n";
  webpage+="    .radio-btn_green label {\n";
  webpage+="      background-color: lightgreen;\n";
  webpage+="      border: 0.5px solid green ;\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .radio-btn_violet input[type=\"radio\"]:checked+label {\n";
  webpage+="      background-color: darkviolet;\n";
  webpage+="    }\n";
  webpage+="    .radio-btn_violet label {\n";
  webpage+="      background-color: palevioletred;\n";
  webpage+="      border: 0.5px solid darkviolet ;\n";
  webpage+="\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .checkbox {\n";
  webpage+="      display: none;\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .checkbox + label {\n";
  webpage+="      display: inline-block;\n";
  webpage+="      background-color: LightGray;\n";
  webpage+="      color: #fff;\n";
  webpage+="      padding: 4px;\n";
  webpage+="      cursor: pointer;\n";
  webpage+="      font-family: Arial;\n";
  webpage+="      font-size: 16px;\n";
  webpage+="      border-radius: 15px;\n";
  webpage+="      border: 0.5px solid LightSlateGray;\n";
  webpage+="\n";
  webpage+="    }\n";
  webpage+="\n";
  webpage+="    .checkbox + label:active,\n";
  webpage+="    .checkbox:checked + label {\n";
  webpage+="      background-color: LightSlateGray;\n";
  webpage+="      border: 0.5px solid LightSlateGray;\n";
  webpage+="    }\n";
  webpage+="  </style>\n";
  webpage+="  <head>\n";
  webpage+="    <title>Gree AC</title>\n";
  webpage+="  </head>\n";
  webpage+="\n";
  webpage+="      <p>\n";
  webpage+="        <center>\n";
  webpage+="          <h2>\n";
  webpage+="            Current Temp: <span id=\"room_temp\"></span>";
  webpage+=DHT.temperature;
  webpage+="&degC\n";
  webpage+="          <br/>\n";
  webpage+="            Current Humidity: <span id=\"room_humidity\"></span>";
  webpage+=DHT.humidity;
  webpage+="&#37\n";
  webpage+="          </h2>\n";
  webpage+="        </center>\n";
  webpage+="      </p>\n";
  webpage+="\n";
  webpage+="    <center>\n";
  webpage+="    <div class=\"remote_box\">\n";
  webpage+="      <p>\n";
  webpage+="        <span id=\"showtemp\"></span>&degC\n";
  webpage+="        <br>\n";
  webpage+="        <input type=\"range\" min=\"16\" max=\"31\" value=\"21\" class=\"slider\" id=\"temperature\">\n";
  webpage+="      </p>\n";
  webpage+="        <p>\n";
  webpage+="          <div class=\"box box_green\">\n";
  webpage+="            <p>Mode</p>\n";
  webpage+="            <p>\n";
  webpage+="            <form id=\"ac_mode\" name=\"ac_mode\" class=\"radio-btn radio-btn_green\">\n";
  webpage+="              <input type=\"radio\" name=\"Mode\" id=\"M_0\" value=\"0\" checked=\"checked\"/>\n";
  webpage+="              <label for=\"M_0\">Auto</label>\n";
  webpage+="              <input type=\"radio\" name=\"Mode\" id=\"M_4\" value=\"4\" />\n";
  webpage+="              <label for=\"M_4\">Heat</label>\n";
  webpage+="              <input type=\"radio\" name=\"Mode\" id=\"M_1\" value=\"1\" />\n";
  webpage+="              <label for=\"M_1\">Cool</label>\n";
  webpage+="              <input type=\"radio\" name=\"Mode\" id=\"M_2\" value=\"2\" />\n";
  webpage+="              <label for=\"M_2\">Dry</label>\n";
  webpage+="              <input type=\"radio\" name=\"Mode\" id=\"M_3\" value=\"3\" />\n";
  webpage+="              <label for=\"M_3\">Fan</label>\n";
  webpage+="            </form>\n";
  webpage+="          </p>\n";
  webpage+="        </div>\n";
  webpage+="      </p>\n";
  webpage+="\n";
  webpage+="      <p>\n";
  webpage+="        <div class=\"box box_violet\">\n";
  webpage+="          <p>Direction</p>\n";
  webpage+="          <form id=\"Flap\" name=\"Flap\" class=\"radio-btn radio-btn_violet\">\n";
  webpage+="              <p>\n";
  webpage+="              <input type=\"radio\" name=\"flap\" id=\"F_2\" value=\"2\" />\n";
  webpage+="              <label for=\"F_2\">Highest</label>\n";
  webpage+="              <input type=\"radio\" name=\"flap\" id=\"F_3\" value=\"3\" />\n";
  webpage+="              <label for=\"F_3\">High</label>\n";
  webpage+="              <input type=\"radio\" name=\"flap\" id=\"F_4\" value=\"4\" />\n";
  webpage+="              <label for=\"F_4\">Middle</label>\n";
  webpage+="              <input type=\"radio\" name=\"flap\" id=\"F_5\" value=\"5\" />\n";
  webpage+="              <label for=\"F_5\">Low</label>\n";
  webpage+="              <input type=\"radio\" name=\"flap\" id=\"F_6\" value=\"6\" />\n";
  webpage+="              <label for=\"F_6\">Lowest</label>\n";
  webpage+="              </p>\n";
  webpage+="              <p>\n";
  webpage+="              <input type=\"radio\" name=\"flap\" value=\"1\" id=\"F_1\" checked=\"checked\"/>\n";
  webpage+="              <label for=\"F_1\">Auto</label>\n";
  webpage+="              <input type=\"radio\" name=\"flap\" id=\"F_7\" value=\"7\" />\n";
  webpage+="              <label for=\"F_7\">Low Auto</label>\n";
  webpage+="              <input type=\"radio\" name=\"flap\" id=\"F_11\" value=\"11\" />\n";
  webpage+="              <label for=\"F_11\">High Auto</label>\n";
  webpage+="              <input type=\"radio\" name=\"flap\" id=\"F_9\" value=\"9\"  />\n";
  webpage+="              <label for=\"F_9\">Mid Auto</label>\n";
  webpage+="              </p>\n";
  webpage+="          </form>\n";
  webpage+="        </div>\n";
  webpage+="      </p>\n";
  webpage+="        <p>\n";
  webpage+="        <div class=\"box box_blue\">\n";
  webpage+="          <p>Fan Speed</p>\n";
  webpage+="          <form id=\"FanMode\" name=\"FanMode\" class=\"radio-btn radio-btn_blue\">\n";
  webpage+="            <p>\n";
  webpage+="            <input type=\"radio\" name=\"fan\" value=\"0\" id=\"S_0\" checked=\"checked\"/>\n";
  webpage+="            <label for=\"S_0\">Auto</label>\n";
  webpage+="            <input type=\"radio\" name=\"fan\" id=\"S_1\" value=\"1\"/>\n";
  webpage+="            <label for=\"S_1\">Slow</label>\n";
  webpage+="            <input type=\"radio\" name=\"fan\" id=\"S_2\" value=\"2\"/>\n";
  webpage+="            <label for=\"S_2\">Mid</label>\n";
  webpage+="            <input type=\"radio\" name=\"fan\" id=\"S_3\" value=\"3\"/>\n";
  webpage+="            <label for=\"S_3\">Max</label>\n";
  webpage+="            </p>\n";
  webpage+="          </form>\n";
  webpage+="        </div>\n";
  webpage+="        </p>\n";
  webpage+="\n";
  webpage+="        <p>\n";
  webpage+="          <div class=\"box box_gray\">\n";
  webpage+="            <p>Options</p>\n";
  webpage+="              <form id=\"options\" name=\"options\">\n";
  webpage+="                <p>\n";
  webpage+="                <input type=\"checkbox\" name=\"light\" class=\"checkbox\" id=\"light\"/>\n";
  webpage+="                <label for=\"light\">Light</label>\n";
  webpage+="                <input type=\"checkbox\" name=\"turbo\" class=\"checkbox\" id=\"turbo\"/>\n";
  webpage+="                <label for=\"turbo\">Turbo</label>\n";
  webpage+="                <input type=\"checkbox\" name=\"xfan\" class=\"checkbox\" id=\"xfan\"/>\n";
  webpage+="                <label for=\"xfan\">X-Fan</label>\n";
  webpage+="                <input type=\"checkbox\" name=\"sleep\" class=\"checkbox\" id=\"sleep\"/>\n";
  webpage+="                <label for=\"sleep\">Sleep</label>\n";
  webpage+="                </p>\n";
  webpage+="              </form>\n";
  webpage+="          </div>\n";
  webpage+="        </p>\n";
  webpage+="\n";
  webpage+="    <div class=\"alert\" id=\"alert_message\">\n";
  webpage+="      <span id=\"alert_message_txt\">____</span>\n";
  webpage+="    </div>\n";
  webpage+="    <br/>\n";
  webpage+="    <div>\n";
  webpage+="        <button class=\"btn btn_blue\" onclick=\"sendCommand()\">Send Command</button>\n";
  webpage+="        <button class=\"btn btn_red\" onclick=\"turnoff()\">Turn Off</button>\n";
  webpage+="    </div>\n";
  webpage+="  </div>\n";
  webpage+="</center>\n";
  webpage+="    <script>\n";
  webpage+="      var slider = document.getElementById(\"temperature\");\n";
  webpage+="      var output = document.getElementById(\"showtemp\");\n";
  webpage+="      output.innerHTML = slider.value;\n";
  webpage+="\n";
  webpage+="      slider.oninput = function() {\n";
  webpage+="        output.innerHTML = this.value;\n";
  webpage+="      }\n";
  webpage+="    </script>\n";
  webpage+="    <script>\n";
  webpage+="    function alrt_message(text, timeout, bgcolor, hide) {\n";
  webpage+="      var alert_msg=document.getElementById(\"alert_message\");\n";
  webpage+="      var alert_text=document.getElementById(\"alert_message_txt\");\n";
  webpage+="      alert_text.innerHTML=text;\n";
  webpage+="      alert_message.style.backgroundColor=bgcolor;\n";
  webpage+="      alert_msg.style.visibility=\"visible\";\n";
  webpage+="      if (hide) {\n";
  webpage+="        window.setTimeout(function() {\n";
  webpage+="          alert_msg.style.visibility=\"hidden\";\n";
  webpage+="        }, timeout);\n";
  webpage+="      }\n";
  webpage+="    }\n";
  webpage+="    function sendCommand() {\n";
  webpage+="        var light = 0;\n";
  webpage+="        var turbo = 0;\n";
  webpage+="        var sleep = 0;\n";
  webpage+="        var xfan = 0;\n";
  webpage+="        var on = 1;\n";
  webpage+="        var temperature = document.getElementById(\"temperature\").value;\n";
  webpage+="        var fanspeed = document.FanMode.fan.value;\n";
  webpage+="        var acmode =  document.ac_mode.Mode.value;\n";
  webpage+="        var flap =  document.Flap.flap.value;\n";
  webpage+="        var flap_automatic = 0;\n";
  webpage+="        if (flap>6 || flap==1) {\n";
  webpage+="          flap_automatic = 1;\n";
  webpage+="          console.log(\"Flap auto enabled.\")\n";
  webpage+="        }\n";
  webpage+="\n";
  webpage+="        var light_checkbox = document.querySelector('#light:checked');\n";
  webpage+="        if (light_checkbox) {\n";
  webpage+="          light = 1;\n";
  webpage+="          console.log(\"Light On\");\n";
  webpage+="        }\n";
  webpage+="        var turbo_checkbox = document.querySelector('#turbo:checked');\n";
  webpage+="        if (turbo_checkbox) {\n";
  webpage+="          turbo = 1;\n";
  webpage+="          console.log(\"Turbo On\");\n";
  webpage+="        }\n";
  webpage+="        var sleep_checkbox =  document.querySelector('#sleep:checked');\n";
  webpage+="        if(sleep_checkbox) {\n";
  webpage+="          sleep = 1;\n";
  webpage+="          console.log(\"Sleep on\");\n";
  webpage+="        }\n";
  webpage+="        var xfan_checkbox =  document.querySelector('#xfan:checked');\n";
  webpage+="        if(xfan_checkbox) {\n";
  webpage+="          xfan = 1;\n";
  webpage+="          console.log(\"X-Fan on\");\n";
  webpage+="        }\n";
  webpage+="\n";
  webpage+="        //command format:[mode, temp, fan_speed,\n";
  webpage+="        //                flap_auto_flag, flap,\n";
  webpage+="        //                light, turbo, xfan, sleep, on_off]\n";
  webpage+="        var command = [acmode, temperature, fanspeed, flap_automatic,flap, light, turbo, xfan, sleep, on];\n";
  webpage+="        //POST\n";
  webpage+="        var xhr = new XMLHttpRequest();\n";
  webpage+="        xhr.open(\"POST\", \"/acremote\");\n";
  webpage+="        xhr.setRequestHeader(\"Content-type\", \"application/x-www-form-urlencoded\");\n";
  webpage+="        xhr.timeout=6000;\n";
  webpage+="        xhr.ontimeout = function() {\n";
  webpage+="          alrt_message(\"Timed Out.\",3000,\"red\");\n";
  webpage+="        }\n";
  webpage+="        xhr.send(\"command=\"+command);\n";
  webpage+="        alrt_message(\"Sending Command....\",5000,\"gray\",false);\n";
  webpage+="        xhr.onreadystatechange = function() {\n";
  webpage+="          if(xhr.readyState == XMLHttpRequest.DONE && xhr.status == 200) {\n";
  webpage+="            alrt_message(\"Command Sent.\", 3000,\"green\", true);\n";
  webpage+="          }\n";
  webpage+="          else if (xhr.readyState== XMLHttpRequest.DONE && xhr.status!=200){\n";
  webpage+="            alrt_message(\"Error.\",3000,\"red\",true);\n";
  webpage+="\n";
  webpage+="          }\n";
  webpage+="        }\n";
  webpage+="\n";
  webpage+="\n";
  webpage+="    }\n";
  webpage+="    </script>\n";
  webpage+="    <script>\n";
  webpage+="    function alrt_message(text, timeout, bgcolor, hide) {\n";
  webpage+="      var alert_msg=document.getElementById(\"alert_message\");\n";
  webpage+="      var alert_text=document.getElementById(\"alert_message_txt\");\n";
  webpage+="      alert_text.innerHTML=text;\n";
  webpage+="      alert_message.style.backgroundColor=bgcolor;\n";
  webpage+="      alert_msg.style.visibility=\"visible\";\n";
  webpage+="      if (hide) {\n";
  webpage+="        window.setTimeout(function() {\n";
  webpage+="          alert_msg.style.visibility=\"hidden\";\n";
  webpage+="        }, timeout);\n";
  webpage+="      }\n";
  webpage+="    }\n";
  webpage+="    function turnoff() {\n";
  webpage+="        var command=[0,0,0,0,0,0,0,0,0,0];\n";
  webpage+="        var alert_msg=document.getElementById(\"alert_message\");\n";
  webpage+="        var alert_text=document.getElementById(\"alert_message_txt\");\n";
  webpage+="        var xhr = new XMLHttpRequest();\n";
  webpage+="        xhr.open(\"POST\", \"/acremote\");\n";
  webpage+="        xhr.setRequestHeader(\"Content-type\", \"application/x-www-form-urlencoded\");\n";
  webpage+="        xhr.timeout=6000;\n";
  webpage+="        xhr.ontimeout = function() {\n";
  webpage+="          alrt_message(\"Timed Out.\", 3000,\"red\", true);\n";
  webpage+="        }\n";
  webpage+="        xhr.send(\"command=\"+command);\n";
  webpage+="        alrt_message(\"Sending Command...\", 3000,\"gray\",false);\n";
  webpage+="        alert_text.innerHTML=\"Sending Command...\";\n";
  webpage+="        alert_msg.style.visibility=\"visible\";\n";
  webpage+="        xhr.onreadystatechange = function() {\n";
  webpage+="          if(xhr.readyState == XMLHttpRequest.DONE && xhr.status == 200) {\n";
  webpage+="            alrt_message(\"Command Sent.\", 5000,\"green\",true);\n";
  webpage+="          }\n";
  webpage+="          else if (xhr.readyState== XMLHttpRequest.DONE && xhr.status!=200){\n";
  webpage+="            alrt_message(\"Error.\", 3000,\"red\",true);\n";
  webpage+="          }\n";
  webpage+="        }\n";
  webpage+="    }\n";
  webpage+="    </script>\n";
  webpage+="    <script>\n";
  webpage+="    function readstate() {\n";
  webpage+="      var oldstate=";
  webpage+=string_state;
  webpage+=";\n";
  webpage+="      //command format:[mode, temp, fan_speed,\n";
  webpage+="      //                flap_auto_flag, flap,\n";
  webpage+="      //                light, turbo, xfan, sleep, on_off]\n";
  webpage+="      // See forms code.\n";
  webpage+="      // All radiobuttons in Mode have id=M_XX where X is the value (from IRRemote)\n";
  webpage+="      // All radiobuttons in Direction/Flap have id=F_XX where XX is the value (from IRRemote)\n";
  webpage+="      // All radiobuttons in FanSpeed have id=S_XX where XX is the value (from IRRemote)\n";
  webpage+="      var mode = document.getElementById(\"M_\"+oldstate[0]);\n";
  webpage+="      var fanspeed=document.getElementById(\"S_\"+oldstate[2]);\n";
  webpage+="      var direction = document.getElementById(\"F_\"+oldstate[4]);\n";
  webpage+="      var options=document.options;\n";
  webpage+="      var temp = document.getElementById(\"temperature\");\n";
  webpage+="      var output = document.getElementById(\"showtemp\");\n";
  webpage+="\n";
  webpage+="      //Set UI to last settings\n";
  webpage+="      mode.checked=true;\n";
  webpage+="      fanspeed.checked=true;\n";
  webpage+="      direction.checked=true;\n";
  webpage+="      temp.value=oldstate[1];\n";
  webpage+="      output.innerHTML = oldstate[1];\n";
  webpage+="      // Set also the \"Options\"\n";
  webpage+="      for(i=0;i<options.length;i++) {\n";
  webpage+="        if(oldstate[5+i]==1) {\n";
  webpage+="          options[i].checked=true;\n";
  webpage+="        }\n";
  webpage+="      }\n";
  webpage+="    }\n";
  webpage+="    </script>\n";
  webpage+="  </body>\n";
  webpage+="</html>\n";
  webpage+="";

server.send(200, "text/html", webpage);
}

// HTTP POST that gets command from WebUI and sends it to IR
void postacremote(){
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication(DIGEST_AUTH,www_realm,www_error_message);
  }
  String command = server.arg("command");
  char tochar[command.length()+1];
  command.toCharArray(tochar, command.length()+1);
  Serial.println(command);
  char *current_int =  strtok(tochar,",");
  int command_index=0;
  uint16_t command_received[CMD_PARAMS];
  while(current_int!=NULL && command_index<CMD_PARAMS) {
    command_received[command_index++]=atoi(current_int);

    current_int=strtok(NULL, ",");
  }
  for (int j=0; j<CMD_PARAMS;j++) {
    Serial.print("Param[");
    Serial.print(j);
    Serial.print("]: ");
    Serial.println(command_received[j]);
  }
  //command format:[mode, temp, fan_speed,
  //                flap_auto_flag, flap,
  //                light, turbo, xfan, sleep, on_off]
  // Executed only if we are turning on the AC..
  if(command_received[9]==1) {
    ac.setMode(command_received[0]);
    ac.setTemp(command_received[1]);
    ac.setFan(command_received[2]);
    ac.setSwingVertical(command_received[3],command_received[4]);
    ac.setLight(command_received[5]);
    ac.setTurbo(command_received[6]);
    ac.setXFan(command_received[7]);
    ac.setSleep(command_received[8]);
  }
  ac.setPower(command_received[9]);
  //Save params if we are not turning off..
  if(command_received[9]==1) {
    for(int h=0;h<CMD_PARAMS-1;h++) {
      state[h]=command_received[h];
    }
  }
  //And save also in state if last command was
  // a command or Off
  state[9]=command_received[9];
  int send_cmd_repeat = 0;
  while(send_cmd_repeat<CMD_REPEAT) {
    ac.send();
    delay(1500);
    send_cmd_repeat++;
  }
  EEPROM.put(0,state);
  EEPROM.commit();

  server.send(200, "text/plain", "ok");

}
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  //WiFi.config(ip, gateway, subnet);
  Serial.println("");
  // EEPROM for state[]
  EEPROM.begin(CMD_PARAMS*sizeof(uint8_t));
  EEPROM.get(0,state);
  for (int j=0; j<CMD_PARAMS;j++) {
    Serial.print("setup:state[");
    Serial.print(j);
    Serial.print("]: ");
    Serial.println(state[j]);
  }
  state_check();
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  ac.begin();
  // IR send last known state
  ac.setMode(state[0]);
  ac.setTemp(state[1]);
  ac.setFan(state[2]);
  ac.setSwingVertical(state[3],state[4]);
  ac.setLight(state[5]);
  ac.setTurbo(state[6]);
  ac.setXFan(state[7]);
  ac.setSleep(state[8]);
  ac.setPower(state[9]);

  Serial.println("Done.");
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.onNotFound(handleNotFound);
  server.on("/acremote", HTTP_POST, postacremote);
  server.on("/", handleAC);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
