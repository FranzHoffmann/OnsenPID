#ifndef WEB_H
#define WEB_H

#include <ESP8266WebServer.h>
#include <Logfile.h>
#include <LittleFS.h>
#include <Process.h>

void setup_webserver(ESP8266WebServer *s);

void set_selected_recipe_no(int no);


extern FS filesystem;
extern Process sm;

#endif