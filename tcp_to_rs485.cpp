#include <iostream>
#include <iomanip>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <chrono>

using namespace boost::asio;
using namespace std;

const int SERIAL_BAUD_RATE = 9600;
const char SERIAL_PORT[] = "/dev/tty.usbserial-10";
const char TCP_SERVER_IP[] = "192.168.1.32";
const int TCP_SERVER_PORT = 8899;
const int TCP_TIMEOUT = 5;
const int SERIAL_TIMEOUT = 5;

class TCPToRS485Client {
public:
    TCPToRS485Client(io_context& io_ctx)
        : socket_(io_ctx), serial_(io_ctx, SERIAL_PORT), resolver_(io_ctx),
          tcp_timer_(io_ctx), serial_timer_(io_ctx), waiting_for_data_(true) {
        configure_serial();
        connect_to_server();
        start_serial_read();
    }

private:
    void configure_serial() {
        serial_.set_option(serial_port::baud_rate(SERIAL_BAUD_RATE));
        serial_.set_option(serial_port::character_size(8));
        serial_.set_option(serial_port::parity(serial_port::parity::none));
        serial_.set_option(serial_port::stop_bits(serial_port::stop_bits::one));
    }

    void connect_to_server() {
        cout << "[INFO] Connecting to TCP server..." << endl;
        
        tcp_timer_.expires_after(std::chrono::seconds(TCP_TIMEOUT));
        tcp_timer_.async_wait([this](boost::system::error_code ec) {
            if (!ec) {
                cout << "[ERROR] TCP connection timeout! Retrying..." << endl;
                socket_.close();
                connect_to_server();
            }
        });

        resolver_.async_resolve(TCP_SERVER_IP, to_string(TCP_SERVER_PORT),
            [this](boost::system::error_code ec, ip::tcp::resolver::results_type results) {
                if (!ec) {
                    socket_.async_connect(*results.begin(), [this](boost::system::error_code ec) {
                        if (!ec) {
                            cout << "[INFO] Connected to TCP server " << TCP_SERVER_IP << ":" << TCP_SERVER_PORT << endl;
                            tcp_timer_.cancel();
                            start_tcp_read();
                        } else {
                            cerr << "[ERROR] Connection failed: " << ec.message() << endl;
                            connect_to_server();
                        }
                    });
                } else {
                    cerr << "[ERROR] Resolve failed: " << ec.message() << endl;
                    connect_to_server();
                }
            });
    }

    void start_tcp_read() {
        socket_.async_read_some(buffer(tcp_data_, max_length),
            [this](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    log_received_tcp_data(tcp_data_, length);
                    tcp_received_data_ = string(tcp_data_, length);
                    attempt_rs485_write();
                    start_tcp_read();
                } else {
                    cerr << "[ERROR] TCP Read failed: " << ec.message() << endl;
                    connect_to_server();
                }
            });
    }

    void start_serial_read() {
        serial_timer_.expires_after(std::chrono::seconds(SERIAL_TIMEOUT));
        serial_timer_.async_wait([this](boost::system::error_code ec) {
            if (!ec) {
                cout << "[WARNING] No RS485 data received. Attempting to send TCP data." << endl;
                attempt_rs485_write();
                start_serial_read();
            }
        });

        serial_.async_read_some(buffer(serial_data_, max_length),
            [this](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    serial_timer_.cancel();
                    log_received_rs485_data(serial_data_, length);
                    send_to_tcp(serial_data_, length);
                    waiting_for_data_ = true;
                    start_serial_read();
                } else {
                    cerr << "[ERROR] Serial Read failed: " << ec.message() << endl;
                    open_serial();
                }
            });
    }

    void attempt_rs485_write() {
        if (!tcp_received_data_.empty()) {
            cout << "[INFO] No RS485 data received recently, sending TCP data to RS485." << endl;
            boost::asio::write(serial_, buffer(tcp_received_data_));
            log_sent_rs485_data(tcp_received_data_.c_str(), tcp_received_data_.size());
            tcp_received_data_.clear();
            waiting_for_data_ = false;
        }
    }

    void send_to_tcp(const char* data, size_t length) {
        boost::asio::write(socket_, buffer(data, length));
        log_sent_tcp_data(data, length);
    }

    void open_serial() {
        try {
            serial_.open(SERIAL_PORT);
            configure_serial();
            cout << "[INFO] Reopened serial port: " << SERIAL_PORT << endl;
        } catch (exception& e) {
            cerr << "[ERROR] Serial reopen failed: " << e.what() << endl;
        }
    }

    void log_received_tcp_data(const char* data, size_t length) {
        cout << "[TCP] Received " << length << " bytes: ";
        print_hex(data, length);
    }

    void log_sent_rs485_data(const char* data, size_t length) {
        cout << "[RS485] Sent " << length << " bytes: ";
        print_hex(data, length);
    }

    void log_received_rs485_data(const char* data, size_t length) {
        cout << "[RS485] Received " << length << " bytes: ";
        print_hex(data, length);
    }

    void log_sent_tcp_data(const char* data, size_t length) {
        cout << "[TCP] Sent " << length << " bytes: ";
        print_hex(data, length);
    }

    void print_hex(const char* data, size_t length) {
        cout << hex << setfill('0');
        for (size_t i = 0; i < length; i++) {
            cout << " " << setw(2) << (static_cast<unsigned>(data[i]) & 0xFF);
        }
        cout << dec << endl;
    }

    ip::tcp::socket socket_;
    serial_port serial_;
    ip::tcp::resolver resolver_;
    steady_timer tcp_timer_;
    steady_timer serial_timer_;
    
    enum { max_length = 1024 };
    char tcp_data_[max_length];
    char serial_data_[max_length];
    string tcp_received_data_;
    bool waiting_for_data_;
};

int main() {
    try {
        io_context io_ctx;
        TCPToRS485Client client(io_ctx);
        io_ctx.run();
    } catch (exception& e) {
        cerr << "[FATAL] Exception: " << e.what() << "\n";
    }

    return 0;
}
