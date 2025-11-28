#define ESP32_CONP_OTA_USE_WEBSOCKETS
#define ESP_CONFIG_PAGE_ENABLE_LOGGING

#include <Arduino.h>
#include <JPEGDEC.h>
#include "driver/rtc_io.h"

#include "FS.h"
#include "SD_MMC.h"
#include <WiFi.h>
#include "time.h"
#include "ESP-FTP-Server-Lib.h"
#include "FTPFilesystem.h"
#include <WebServer.h>
#include "general/image_util.h"
#include <general/secrets.h>
#include <general/cam_config.h>
#include <esp-config-page.h>

#define FRAME_CAPTURE_INTERVAL_S 2000
#define LUMINOSITY_PIN 12
#define LUMINOSITY_THRESHOLD 400

unsigned long snapCounter = 0; // -FRAME_CAPTURE_INTERVAL_S * 1000;
bool cameraInit = false;
bool timeInit = false;

FTPServer ftp;
WebServer server(8080);

JPEGDEC jpegdec;

uint32_t readLuminosity()
{
    WiFi.disconnect(true);
    uint32_t lum = analogReadMilliVolts(LUMINOSITY_PIN);
    Serial.printf("Luminosity: %lu\n", lum);
    WiFi.begin();
    return lum;
}

bool shouldUseFlash()
{
    return readLuminosity() < LUMINOSITY_THRESHOLD;
}

void saveCrops(char *foldername, char *filenameBuffer, size_t filenameBufferSize, camera_fb_t *fb)
{
    snprintf(filenameBuffer, filenameBufferSize, "%s/log.txt", foldername);
    File logFile = SD_MMC.open(filenameBuffer, FILE_WRITE);

    IMAGE_UTIL::ImageDimensions imageDimensions;
    if (!IMAGE_UTIL::getImageDimensions(fb->buf, fb->len, &imageDimensions) == IMAGE_UTIL::OK)
    {
        logFile.println("Image dimensions extraction error.");
        return;
    }

    const CamConfig::FrameConfig frameConfig = CamConfig::getFrameConfig();

    size_t decodedSize = 0;
    IMAGE_UTIL::calcRgb888FrameSize(
        fb->buf,
        fb->len,
        frameConfig.startFrameX,
        frameConfig.startFrameY,
        frameConfig.frameWidth,
        frameConfig.frameHeight,
        &decodedSize);
    decodedSize += 1024;
    if (decodedSize == 0)
    {
        logFile.println("Error calculating decode buffer size.");
        return;
    }

    auto *decodeBuf = (uint8_t*) ps_malloc(decodedSize);
    if (decodeBuf == nullptr)
    {
        logFile.println("Failed to acquire crop decode buffer.");
        return;
    }

    int decodedWidth = 0;
    int decodedHeight = 0;

    logFile.printf("Starting frame cropping, frame count x=%d, y=%d, total=%d\n", frameConfig.frameCountX, frameConfig.frameCountY, frameConfig.frameCountX*frameConfig.frameCountY);

    for (int i = 0; i < frameConfig.frameCountX; i++)
    {
        for (int j = 0; j < frameConfig.frameCountY; j++)
        {
            int currentX = frameConfig.startFrameX + ((frameConfig.frameWidth / 2) * i);
            int currentY = frameConfig.startFrameY + ((frameConfig.frameHeight / 2) * j);
            logFile.printf("Getting crop for pos x=%d,y=%d\n", currentX, currentY);

            if (currentX >= imageDimensions.width || currentY >= imageDimensions.height)
            {
                break;
            }

            size_t decodedWritten = 0;

            IMAGE_UTIL::Status status = IMAGE_UTIL::getImageFrameRgb888(
                fb->buf,
                fb->len,
                currentX,
                currentY,
                frameConfig.frameWidth,
                frameConfig.frameHeight,
                decodeBuf,
                decodedSize,
                &decodedWritten,
                &decodedWidth,
                &decodedHeight);

            if (status != IMAGE_UTIL::OK)
            {
                logFile.printf("Error getting frame crop: %d\n", status);
                break;
            }

            logFile.printf("Decoded %zu bytes\n", decodedWritten);
            snprintf(filenameBuffer, filenameBufferSize, "%s/x%d__y%d.rgb888", foldername, i, j);
            File file = SD_MMC.open(filenameBuffer, FILE_WRITE);
            file.write(decodeBuf, decodedWritten);
            file.close();

            logFile.println("Reencoding image to jpeg.");
            JPEG_DECODE_UTIL::rgb_to_bgr888(decodeBuf, decodedWritten);
            size_t jpegOutSize = 0;
            uint8_t *jpegOut = nullptr;
            bool reencoded = fmt2jpg(decodeBuf, decodedWritten, decodedWidth, decodedHeight, PIXFORMAT_RGB888, 247, &jpegOut, &jpegOutSize);
            if (reencoded)
            {
                snprintf(filenameBuffer, filenameBufferSize, "%s/x%d__y%d.jpg", foldername, i, j);
                file = SD_MMC.open(filenameBuffer, FILE_WRITE);
                file.write(jpegOut, jpegOutSize);
                file.close();
                logFile.println("Reencoded jpeg written.");
            }
            else
            {
                logFile.println("Reencoding image to jpeg failed.");
            }
        }
    }

    free(decodeBuf);
    logFile.println("Finished crops.");
    logFile.close();
}

void snap(char* foldername, size_t foldernameBufferSize, const char* folder, CamConfig::CamFlashMode flashMode)
{
    if (!cameraInit)
    {
        return;
    }

    unsigned long timer = millis();
    Serial.println("Taking picture.");

    if (flashMode == CamConfig::ON || (flashMode == CamConfig::AUTO && shouldUseFlash()))
    {
        digitalWrite(FLASH_PIN, HIGH);
        delay(100);
    }

    camera_fb_t* fb = esp_camera_fb_get();
    delay(10);
    esp_camera_fb_return(fb);
    delay(10);
    fb = esp_camera_fb_get();
    delay(10);
    esp_camera_fb_return(fb);
    delay(50);

    fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("Frame buffer could not be acquired");
        return;
    }

    delay(10);
    digitalWrite(FLASH_PIN, LOW);

    Serial.printf("Pic len: %zu\n", fb->len);
    tm timeinfo{};
    getLocalTime(&timeinfo);

    if (!SD_MMC.exists(folder))
    {
        SD_MMC.mkdir(folder);
    }

    snprintf(foldername,
             foldernameBufferSize,
             "%s/%d_%02d_%02d__%02d_%02d_%02d",
             folder,
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);

    if (!SD_MMC.exists(foldername))
    {
        SD_MMC.mkdir(foldername);
    }

    char filenameBuffer[150]{};
    snprintf(filenameBuffer, sizeof(filenameBuffer), "%s/full.jpg", foldername);

    File file = SD_MMC.open(filenameBuffer, FILE_WRITE);
    file.write(fb->buf, fb->len);
    file.close();

    saveCrops(foldername, filenameBuffer, sizeof(filenameBuffer), fb);
    snprintf(filenameBuffer, sizeof(filenameBuffer), "%s/full.jpg", foldername);
    snprintf(foldername, foldernameBufferSize, "%s", filenameBuffer);

    if (!SD_MMC.exists("/luminosity"))
    {
        SD_MMC.mkdir("/luminosity");
    }

    if (flashMode == CamConfig::AUTO)
    {
        const uint32_t lum = readLuminosity();
        char lumFilename[128]{};
        snprintf(lumFilename,
                 sizeof(lumFilename),
                 "/luminosity/%d_%02d_%02d__%02d_%02d_%02d__%lu.lum",
                 timeinfo.tm_year + 1900,
                 timeinfo.tm_mon + 1,
                 timeinfo.tm_mday,
                 timeinfo.tm_hour,
                 timeinfo.tm_min,
                 timeinfo.tm_sec,
                 lum);
        File lumFile = SD_MMC.open(lumFilename, FILE_WRITE);
        lumFile.print(lum);
        lumFile.close();
    }

    esp_camera_fb_return(fb);
    Serial.println("Pic saved.");
    Serial.printf("Time taken: %lu\n---------------------------------\n", millis() - timer);
}

void instant(CamConfig::CamFlashMode flashMode)
{
    if (flashMode == CamConfig::ON || flashMode == CamConfig::AUTO)
    {
        digitalWrite(FLASH_PIN, HIGH);
        delay(50);
    }

    camera_fb_t *fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);

    fb = esp_camera_fb_get();

    delay(15);
    digitalWrite(FLASH_PIN, LOW);

    constexpr char filename[] = "/instant.tmp";
    File file = SD_MMC.open(filename, FILE_WRITE);
    file.write(fb->buf, fb->len);
    file.close();

    esp_camera_fb_return(fb);

    file = SD_MMC.open(filename, FILE_READ);
    server.streamFile(file, "image/jpeg");
    file.close();
}

void instantCrop(CamConfig::CamFlashMode flashMode, CamConfig::FrameConfig frame, int outJpegQuality)
{
    if (flashMode == CamConfig::ON)
    {
        digitalWrite(FLASH_PIN, HIGH);
        delay(100);
    }

    camera_fb_t *fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);

    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);

    fb = esp_camera_fb_get();
    delay(15);
    digitalWrite(FLASH_PIN, LOW);

    IMAGE_UTIL::alignToMcu(fb->buf, fb->len, frame.startFrameX, frame.startFrameY, frame.frameWidth, frame.frameHeight);
    server.sendHeader("X-ACT-X", String(frame.startFrameX));
    server.sendHeader("X-ACT-Y", String(frame.startFrameY));
    server.sendHeader("X-ACT-W", String(frame.frameWidth));
    server.sendHeader("X-ACT-H", String(frame.frameHeight));

    size_t decBufSize = 0;
    IMAGE_UTIL::calcRgb888FrameSize(fb->buf, fb->len, frame.startFrameX, frame.startFrameY, frame.frameWidth, frame.frameHeight, &decBufSize);
    auto *decBuf = (uint8_t*) ps_malloc(decBufSize + 512);
    if (decBuf == nullptr)
    {
        esp_camera_fb_return(fb);
        server.send(500, "text/plain", "Couldn't get instant frame decode buffer.");
        return;
    }

    size_t decWritten = 0;
    int outWidth = 0;
    int outHeight = 0;
    int ret = IMAGE_UTIL::getImageFrameRgb888(
        fb->buf,
        fb->len,
        frame.startFrameX,
        frame.startFrameY,
        frame.frameWidth,
        frame.frameHeight,
        decBuf,
        decBufSize,
        &decWritten,
        &outWidth,
        &outHeight);

    if (ret != IMAGE_UTIL::OK)
    {
        esp_camera_fb_return(fb);
        server.send(500, "text/plain", "Failed to decode frame.");
        free(decBuf);
        return;
    }

    uint8_t *jpegBuf;
    size_t jpegOutSize;
    JPEG_DECODE_UTIL::rgb_to_bgr888(decBuf, decWritten);
    bool encoded = fmt2jpg(decBuf, decWritten, outWidth, outHeight, PIXFORMAT_RGB888, outJpegQuality, &jpegBuf, &jpegOutSize);
    if (!encoded)
    {
        esp_camera_fb_return(fb);
        server.send(500, "text/plain", "Failed to reencode decoded image to jpeg");
        free(decBuf);
        return;
    }

    constexpr char filename[] = "/instant.tmp";
    File file = SD_MMC.open(filename, FILE_WRITE);
    file.write(jpegBuf, jpegOutSize);
    file.close();

    esp_camera_fb_return(fb);
    free(decBuf);
    free(jpegBuf);

    file = SD_MMC.open(filename, FILE_READ);
    server.streamFile(file, "image/jpeg");
    file.close();
}

void setup()
{
    Serial.begin(115200);

    cameraInit = CamConfig::initCamera();
    cameraInit = CamConfig::initSdCard();

    ESP_CONFIG_PAGE::addEnvVar(CamConfig::startXEnvVar);
    ESP_CONFIG_PAGE::addEnvVar(CamConfig::startYEnvVar);
    ESP_CONFIG_PAGE::addEnvVar(CamConfig::frameCountX);
    ESP_CONFIG_PAGE::addEnvVar(CamConfig::frameCountY);
    ESP_CONFIG_PAGE::addEnvVar(CamConfig::frameWidthVar);
    ESP_CONFIG_PAGE::addEnvVar(CamConfig::frameHeightVar);
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(new ESP_CONFIG_PAGE::LittleFSKeyValueStorage("/env"));

    ESP_CONFIG_PAGE::addCustomAction("RESTART", [](ESP_CONFIG_PAGE::WEBSERVER_T &server)
    {
        server.send(200);
        ESP.restart();
    });

    ESP_CONFIG_PAGE::setAPConfig(nodeName, password);
    ESP_CONFIG_PAGE::initModules(&server, username, password, nodeName);

    ftp.addUser(username, password);
    ftp.addFilesystem("SD", &SD_MMC);
    ftp.begin();

    server.on("/snap", HTTP_GET, []()
    {
        if (!cameraInit)
        {
            server.send(503, "text/plain", "cam unavailable");
            return;
        }

        String f = server.arg("f");

        char filename[128]{};
        snap(filename, 128, "/ondemand", (CamConfig::CamFlashMode) f.toInt());

        File file = SD_MMC.open(filename, FILE_READ);
        server.streamFile(file, "image/jpeg");
        file.close();
    });

    server.on("/crop", HTTP_GET, []()
    {
        CamConfig::FrameConfig frame{};
        frame.startFrameX = server.arg("x").toInt();
        frame.startFrameY = server.arg("y").toInt();
        frame.frameWidth = server.arg("w").toInt();
        frame.frameHeight = server.arg("h").toInt();
        instantCrop((CamConfig::CamFlashMode) server.arg("f").toInt(), frame, server.arg("j").toInt());
    });

    server.on("/instant", HTTP_GET, []()
    {
       instant((CamConfig::CamFlashMode) server.arg("f").toInt());
    });

    server.begin();

    Serial.printf("Initialized, IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Psram/heap free size: %zu/%zu\n", ESP.getFreePsram(), ESP.getFreeHeap());
}

void loop()
{
    ESP_CONFIG_PAGE::loop();
    server.handleClient();
    ftp.handle();

    if (WiFi.status() == WL_CONNECTED)
    {
        if (!timeInit)
        {
            // gmt-3
            configTime(-3 * 3600, 0, "pool.ntp.org");
            tm info{};
            timeInit = getLocalTime(&info);
        }
    }

    if (!cameraInit)
    {
        return;
    }

    if (millis() - snapCounter > (FRAME_CAPTURE_INTERVAL_S * 1000))
    {
        char filename[128]{};
        snap(filename, 128, "/pic_empty", CamConfig::AUTO);
        snapCounter = millis();
    }
}
