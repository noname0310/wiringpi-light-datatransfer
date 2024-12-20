#include <iostream>
#include <string>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <cassert>
#include <wiringPi.h>

// #define DEBUG_PRINT_BITS

constexpr int32_t INPUT_PIN = 21;
constexpr int32_t OUTPUT_PIN = 22;

constexpr int32_t ONE_SECOND_IN_MICROSECONDS = 1000000;

constexpr int32_t STOP_BITS = 1;

constexpr int32_t SEND_SIGNAL_TRUE = HIGH;
constexpr int32_t SEND_SIGNAL_FALSE = LOW;

constexpr int32_t RECEIVE_SIGNAL_TRUE = LOW;
constexpr int32_t RECEIVE_SIGNAL_FALSE = HIGH;

constexpr int32_t LENGTH_BITS = 5;
constexpr int32_t MAX_CHUNK_SIZE = 31;

//         __data__
// 0000000110101010000110101010000
//        ^start   ^~~stop

void busyWait(std::chrono::system_clock::time_point end) {
    while (std::chrono::high_resolution_clock::now() < end) {
        // busy wait
    }
}

class SerialSender {
    int32_t delay;

public:
    SerialSender(int32_t baudrate = 9) : delay(ONE_SECOND_IN_MICROSECONDS / baudrate) {
        pinMode(OUTPUT_PIN, OUTPUT);
        digitalWrite(OUTPUT_PIN, SEND_SIGNAL_FALSE);
    }

    void send(const std::string& message) {
        for (char c : message) {
            sendByte(c);
        }
    }

    void fastSend(const std::string& message) {
        sendBytesAsChunk(message.c_str(), message.size());
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
        for (int32_t i = 0; i < 8; ++i) {
            bool bit = (byte >> i) & 0x01;
            digitalWrite(OUTPUT_PIN, bit ? SEND_SIGNAL_TRUE : SEND_SIGNAL_FALSE);
#ifdef DEBUG_PRINT_BITS
            std::cout << (bit ? '1' : '0');
            std::flush(std::cout);
#endif
            next += std::chrono::microseconds(delay);
            busyWait(next);
        }

        // Stop bits
        digitalWrite(OUTPUT_PIN, SEND_SIGNAL_FALSE);
        next += std::chrono::microseconds(delay * STOP_BITS);
        busyWait(next);
    }

    void sendBytes(const char* bytes, uint8_t size) {
        assert(size <= MAX_CHUNK_SIZE);
        
        auto start = std::chrono::high_resolution_clock::now();
        auto next = start;
        
        // Start bit
        digitalWrite(OUTPUT_PIN, SEND_SIGNAL_TRUE);
        next += std::chrono::microseconds(delay);
        busyWait(next);

        // Data bits
        {
            // Data length
            for (int32_t i = 0; i < LENGTH_BITS; ++i) {
                bool bit = (size >> i) & 0x01;
                digitalWrite(OUTPUT_PIN, bit ? SEND_SIGNAL_TRUE : SEND_SIGNAL_FALSE);
#ifdef DEBUG_PRINT_BITS
                std::cout << (bit ? '1' : '0');
                std::flush(std::cout);
#endif
                next += std::chrono::microseconds(delay);
                busyWait(next);
            }

            // Data
            for (int32_t i = 0; i < size; ++i) {
                for (int32_t j = 0; j < 8; ++j) {
                    bool bit = (bytes[i] >> j) & 0x01;
                    digitalWrite(OUTPUT_PIN, bit ? SEND_SIGNAL_TRUE : SEND_SIGNAL_FALSE);
#ifdef DEBUG_PRINT_BITS
                    std::cout << (bit ? '1' : '0');
                    std::flush(std::cout);
#endif
                    next += std::chrono::microseconds(delay);
                    busyWait(next);
                }
            }
        }

        // Stop bits
        digitalWrite(OUTPUT_PIN, SEND_SIGNAL_FALSE);
        next += std::chrono::microseconds(delay * STOP_BITS);
        busyWait(next);
    }

    void sendBytesAsChunk(const char* bytes, uint32_t size) {
        const char* offset = bytes;
        while (0 < size) {
            uint32_t chunk = size < MAX_CHUNK_SIZE ? size : MAX_CHUNK_SIZE;
            sendBytes(offset, chunk);
            offset += chunk;
            size -= chunk;
        }
    }
};

class SerialReceiver {
    int32_t delay;

public:
    SerialReceiver(int32_t baudrate = 9) : delay(ONE_SECOND_IN_MICROSECONDS / baudrate) {
        pinMode(INPUT_PIN, INPUT);
    }

    void receive() {
        for (; ;) {
            char byte = receiveByte();
            std::cout << byte;
            std::flush(std::cout);
        }
    }

    void fastReceive() {
        for (; ;) {
            char bytes[MAX_CHUNK_SIZE];
            //uint8_t size = 
            receiveBytes(bytes, [](char byte) {
                std::cout << byte;
                std::flush(std::cout);
            });
            // for (int32_t i = 0; i < size; ++i) {
            //     std::cout << bytes[i];
            // }
            // std::flush(std::cout);
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
#ifdef DEBUG_PRINT_BITS
            std::cout << (bit ? '1' : '0');
            std::flush(std::cout);
#endif
            next += std::chrono::microseconds(delay);
            busyWait(next);
        }

        // Stop bit start wait
        next += std::chrono::microseconds(delay / 2);
        busyWait(next);

        return byte;
    }

    uint8_t receiveBytes(char* bytes, std::function<void(char)> callback) {
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
        uint8_t size = 0;
        {
            // Data length
            for (int32_t i = 0; i < LENGTH_BITS; ++i) {
                bool bit = digitalRead(INPUT_PIN) == RECEIVE_SIGNAL_TRUE;
                size |= bit << i;
#ifdef DEBUG_PRINT_BITS
                std::cout << (bit ? '1' : '0');
                std::flush(std::cout);
#endif
                next += std::chrono::microseconds(delay);
                busyWait(next);
            }
            std::memset(bytes, 0, size);

            // Data
            for (int32_t i = 0; i < size; ++i) {
                for (int32_t j = 0; j < 8; ++j) {
                    bool bit = digitalRead(INPUT_PIN) == RECEIVE_SIGNAL_TRUE;
                    bytes[i] |= bit << j;
#ifdef DEBUG_PRINT_BITS
                    std::cout << (bit ? '1' : '0');
                    std::flush(std::cout);
#endif
                    next += std::chrono::microseconds(delay);
                    busyWait(next);
                }
                callback(bytes[i]);
            }
        }

        // Stop bit start wait
        next += std::chrono::microseconds(delay / 2);
        busyWait(next);

        return size;
    }
};

enum class Role : int32_t {
    SENDER,
    RECEIVER
};

enum class Mode : int32_t {
    PERBYTE,
    CHUNK,
};

int32_t main(int32_t argc, char* argv[]) {
    // program [role] [baudrate] [mode]
    Role role = argc > 1 ? static_cast<Role>(std::stoi(argv[1])) : Role::SENDER;
    int32_t baudrate = argc > 2 ? std::stoi(argv[2]) : 30;
    Mode mode = argc > 3 ? static_cast<Mode>(std::stoi(argv[3])) : Mode::CHUNK;

    if (wiringPiSetup() == -1) {
        return 1;
    }
    std::cout << "Setup wiringPi" << std::endl;

    if (role == Role::SENDER) {
        SerialSender sender(baudrate);
        for (; ;) {
            std::string message;
            std::cout << "Enter message: ";
            std::getline(std::cin, message);
            message.push_back('\n');

            if (mode == Mode::PERBYTE) {
                sender.send(message);
            } else {
                sender.fastSend(message);
            }
        }
    } else {
        SerialReceiver receiver(baudrate);
        if (mode == Mode::PERBYTE) {
            receiver.receive();
        } else {
            receiver.fastReceive();
        }
    }

    return 0;
}
