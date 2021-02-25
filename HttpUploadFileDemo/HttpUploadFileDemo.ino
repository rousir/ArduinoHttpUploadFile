#include <Arduino.h>

#include "HTTPClient.h"
#include "WiFi.h"

#include <SPIFFS.h>
#include <MD5Builder.h>

const char *ssid = "RTSCAN";               //  your network SSID (name)
const char *password = "yiliguoliduo.1234"; // your network password (use for WPA, or use as key for WEP)

int httpDownloadFile(const char *url, const char *fileName)
{
	File fd = SPIFFS.open(fileName, "w");
	if (!fd) {
		Serial.printf("Can't open %s !\r\n", fileName);
		return -1;
	}

	Serial.printf("DownloadFile %s to %s\n", url, fileName);

	HTTPClient http;
	WiFiClientSecure client;

	http.setTimeout(10000);
	http.begin(client, url);

	int httpCode = http.GET();
	Serial.printf("GET... code: %d\n", httpCode);

	if (httpCode != HTTP_CODE_OK)
	{
		Serial.printf("GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
		return -2;
	}

	// int len = http.getSize();
	int len = 26797;
	Serial.printf("download file size: %d\r\n", len);

	uint8_t buff[128] = {0};
	WiFiClient *stream = http.getStreamPtr();

	while (http.connected() && (len > 0 || len == -1))
	{
		size_t size = stream->available();
		if (size) {
			int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
			fd.write(buff, c);
			if (len > 0) {len -= c;}
		}
		delay(1);
	}

	Serial.printf("save file size: %d\n", fd.size());

	if (len != fd.size()) {
		Serial.printf("save file data len error\n");
	}

	Serial.print("[HTTP] connection closed or file end.\n");

	fd.close();
	http.end();
	return 1;
}

String randomChr(int size)
{
	String ret = "";
	for (int i = 0; i < size; i++)
	{
		if (i % 3 == 0)
		{
			ret += (char)random(48, 57);
		}
		else if (i % 3 == 1)
		{
			ret += (char)random(65, 90);
		}
		else if (i % 3 == 2)
		{
			ret += (char)random(97, 122);
		}
	}
	return ret;
}

int httpUploadFile(const char *fileName, String url)
{
	File fd = SPIFFS.open(fileName);

	if (!fd) {
		Serial.print("open file failed\n");
		return -1;
	}

	// check for : (http: or https:
	int index = url.indexOf(':');
	if (index < 0) {
		Serial.printf("failed to parse protocol\n");
		return -2;
	}

	String protocol = url.substring(0, index);
	url.remove(0, (index + 3)); // remove http:// or https://

	int port = 80;

	if (protocol == "http") {
		port = 80;
	} else if (protocol == "https") {
		port = 443;
	} else {
		Serial.printf("unsupported protocol: %s\n", protocol.c_str());
		return -3;
	}

	String host = "";

	index = url.indexOf('/');
	String _host = url.substring(0, index);
	url.remove(0, index); // remove host part

	// get port
	index = _host.indexOf(':');
	if (index >= 0) {
		host = _host.substring(0, index); // hostname
		_host.remove(0, (index + 1)); // remove hostname + :
		port = _host.toInt(); // get port
	} else {
		host = _host;
	}

	String path = url;

	String boundary = randomChr(32);

	String header = "POST " + path + " HTTP/1.1\r\n";
	header += "Host: " + host + "\r\n";
	header += "Content-Type: multipart/form-data;boundary=" + boundary + "\r\n";
	header += "Connection: keep-alive\r\n";

	String body = "--" + boundary + "\r\n";
	body += "Content-Disposition: form-data;name=\"1\";filename=\"" + String(fileName) + "\"\r\n";
	body += "Content-Type: application/octet-stream\r\n";
	body += "\r\n";

	String bodyend = "\r\n--" + boundary + "--\r\n";

	header += "Content-Length: " + String(body.length() + fd.size() + bodyend.length()) + "\r\n";
	header += "\r\n";

	Serial.print("host:");
	Serial.println(host);

	Serial.print("port:");
	Serial.println(port);

	Serial.print("path:");
	Serial.println(path);

	Serial.println("header:");
	Serial.println(header);

	Serial.println("body:");
	Serial.println(body);

	WiFiClient client;

	if (!client.connect(host.c_str(), port)) {
		Serial.print("open file failed\n");
		return -1;
	}

	client.print(header);
	client.print(body);

	uint8_t *buf = (uint8_t *)malloc(4096);
	int len = 0;

	while (fd.available()) {
		int ret = fd.read(buf, 4096);
		client.write(buf, ret);
		len += ret;
	}
	free(buf);
	
	Serial.printf("write data len: %d\n", len);

	client.print(bodyend);

	Serial.print("response:\n");

	String response_status = client.readStringUntil('\n');

	Serial.print("code:");
	Serial.println(response_status);

	if (response_status.indexOf("200 OK") < 0) {
		Serial.print("response code is not 200\n");
		return 1;
	}else{
		Serial.print("httpUploadFile success\n");
	}

	// while (client.available()) {
	// 	uint8_t ret = client.read();
	// 	Serial.write(ret);
	// }

	Serial.println();

	return 1;
}

void connectToNetwork()
{
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.println(String(" Establishing connection to WiFi..."));
	}
	Serial.println(String(" Connected to network"));

	Serial.println(String(" Mac Address ") + WiFi.macAddress());
	Serial.println(String(" IP Address  ") + WiFi.localIP().toString());
	Serial.println(String(" Gateway IP  ") + WiFi.gatewayIP().toString());
}

String formatBytes(size_t bytes) {
	if (bytes < 1024) {
		return String(bytes) + "B";
	} else if (bytes < (1024 * 1024)) {
		return String(bytes / 1024.0) + "KB";
	} else if (bytes < (1024 * 1024 * 1024)) {
		return String(bytes / 1024.0 / 1024.0) + "MB";
	} else {
		return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
	}
}

void setup()
{
	Serial.begin(115200);

	if (SPIFFS.begin())
	{
		Serial.println("SPIFFS begin success");

		File root = SPIFFS.open("/");
		File file = root.openNextFile();
		while (file) {
			String fileName = file.name();
			size_t fileSize = file.size();
			Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
			file = root.openNextFile();
		}
		Serial.printf("\n");
	} else {
		Serial.println("SPIFFS begin failed");
		SPIFFS.format();
	}

	connectToNetwork();

	// httpDownloadFile(url, "/test.jpg");
	httpUploadFile("/8k.pcm", "http://192.168.2.21/test/");
}

void loop()
{
}
