#include <iostream>
#include <string>
#include <chrono>
#include <wiringPi.h>

constexpr int INPUT_PIN = 22;
constexpr int OUTPUT_PIN = 22;

constexpr int ONE_SECOND_IN_MICROSECONDS = 1000000;

constexpr int STOP_BITS = 1;

constexpr int SEND_SIGNAL_TRUE = HIGH;
constexpr int SEND_SIGNAL_FALSE = LOW;

constexpr int RECEIVE_SIGNAL_TRUE = LOW;
constexpr int RECEIVE_SIGNAL_FALSE = HIGH;

//         __data__
// 0000000110101010000110101010000
//        ^start   ^~~stop

void busyWait(std::chrono::system_clock::time_point end) {
    while (std::chrono::high_resolution_clock::now() < end) {
        // busy wait
    }
}

class SerialSender {
    int delay;

public:
    SerialSender(int baudrate = 9) : delay(ONE_SECOND_IN_MICROSECONDS / baudrate) {
        pinMode(OUTPUT_PIN, OUTPUT);
        digitalWrite(OUTPUT_PIN, SEND_SIGNAL_FALSE);
    }

    void send(const std::string& message) {
        for (char c : message) {
            sendByte(c);
        }
    }

private:
    void sendByte(char byte) {
        auto start = std::chrono::high_resolution_clock::now();
        auto next = start;

        // Start bit
        digitalWrite(OUTPUT_PIN, SEND_SIGNAL_TRUE);
        next += std::chrono::microseconds(delay);
        busyWait(next);

        // Data bits
        for (int i = 0; i < 8; ++i) {
            bool bit = (byte >> i) & 0x01;
            digitalWrite(OUTPUT_PIN, bit ? SEND_SIGNAL_TRUE : SEND_SIGNAL_FALSE);
            next += std::chrono::microseconds(delay);
            busyWait(next);
        }

        // Stop bits
        digitalWrite(OUTPUT_PIN, SEND_SIGNAL_FALSE);
        next += std::chrono::microseconds(delay * STOP_BITS);
        busyWait(next);
    }
};

class SerialReceiver {
    int delay;

public:
    SerialReceiver(int baudrate = 9) : delay(ONE_SECOND_IN_MICROSECONDS / baudrate) {
        pinMode(INPUT_PIN, INPUT);
    }

    void receive() {
        for (; ;) {
            char byte = receiveByte();
            std::cout << byte;
            std::flush(std::cout);
        }
    }

private:
    char receiveByte() {
        // Wait for start bit
        while (digitalRead(INPUT_PIN) == RECEIVE_SIGNAL_FALSE) {
            // busy wait
        }

        auto start = std::chrono::high_resolution_clock::now();
        auto next = start;

        // Wait Start bit
        next += std::chrono::microseconds(delay + delay / 3);
        busyWait(next);

        // Data bits
        char byte = 0;
        for (int i = 0; i < 8; ++i) {
            bool bit = digitalRead(INPUT_PIN) == RECEIVE_SIGNAL_TRUE;
            byte |= bit << i;
            next += std::chrono::microseconds(delay);
            busyWait(next);
        }

        // Stop bit start wait
        next += std::chrono::microseconds(delay / 2);
        busyWait(next);

        return byte;
    }
};

enum class Mode : int {
    SENDER,
    RECEIVER
};

int main(int argc, char* argv[]) {
    // program [mode] [baudrate]
    Mode mode = argc > 1 ? static_cast<Mode>(std::stoi(argv[1])) : Mode::SENDER;
    int baudrate = argc > 2 ? std::stoi(argv[2]) : 38;

    if (wiringPiSetup() == -1) {
        return 1;
    }
    std::cout << "Setup wiringPi" << std::endl;

    if (mode == Mode::SENDER) {
        SerialSender sender(baudrate);
        for (; ;) {
            std::string message;
            std::cout << "Enter message: ";
            std::getline(std::cin, message);
            sender.send(message + '\n');
        }
    } else {
        SerialReceiver receiver(baudrate);
        receiver.receive();
    }

    return 0;
}
