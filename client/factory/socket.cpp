#include <iostream>
#include <string>
#include <random>
#include <unistd.h>
#include <arpa/inet.h>

// Sensor data struct
struct SensorData {
    float pumpPressure;   // MPa, 1.2–2.0
    float pumpVibration;  // mm/s, 0.3–1.5
    float motorCurrent;   // A, 80–120
    float oilLevel;       // %, 30–80
    float pumpTemp;       // °C, 30–55
    float flowRate;       // m³/h, 50–100
};

// Generate random sensor data within specified ranges
SensorData generateSensorData(std::mt19937& gen) {
    std::uniform_real_distribution<float> pumpPressure(1.2, 2.0);
    std::uniform_real_distribution<float> pumpVibration(0.3, 1.5);
    std::uniform_real_distribution<float> motorCurrent(80.0, 120.0);
    std::uniform_real_distribution<float> oilLevel(30.0, 80.0);
    std::uniform_real_distribution<float> pumpTemp(30.0, 55.0);
    std::uniform_real_distribution<float> flowRate(50.0, 100.0);

    return SensorData{
        pumpPressure(gen),
        pumpVibration(gen),
        motorCurrent(gen),
        oilLevel(gen),
        pumpTemp(gen),
        flowRate(gen)
    };
}

// Format sensor data as CSV string
std::string formatSensorData(const SensorData& data) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
        data.pumpPressure, data.pumpVibration, data.motorCurrent,
        data.oilLevel, data.pumpTemp, data.flowRate);
    return buffer;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    std::random_device rd;
    std::mt19937 gen(rd());

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error\n";
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8081);

    // Connect to server (localhost)
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported\n";
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed\n";
        return -1;
    }

    std::cout << "Connected to server. Sending sensor data...\n";
    while (true) {
        SensorData data = generateSensorData(gen);
        std::string message = formatSensorData(data);
        send(sock, message.c_str(), message.size(), 0);
        std::cout << "Sent: " << message;
        sleep(1);
    }

    close(sock);
    return 0;
}