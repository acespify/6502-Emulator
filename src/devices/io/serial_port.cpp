#include "serial_port.h"
#include <iostream>

Serial_Port::Serial_Port(w65c51* acia)
    : m_acia(acia), m_serial(m_io_context)
{}

Serial_Port::~Serial_Port() {
    disconnect();
}

bool Serial_Port::connect(const std::string& port_name, unsigned int baud_rate){
    if (m_connected) disconnect();

    asio::error_code ec;    // the error code bucket
    
    // Open the port, passing 'ec' to prevent it from throwing the error
    m_serial.open(port_name, ec);

    if (ec) {
        std::cerr << "[MAX232] Failed to open " << port_name << ": " << ec.message() << std::endl;
        return false;
    }
        
    // Match the 8-N-1 hardware configuration typical for these systems
    m_serial.set_option(asio::serial_port_base::baud_rate(baud_rate));
    m_serial.set_option(asio::serial_port_base::character_size(8));
    m_serial.set_option(asio::serial_port_base::parity(asio::serial_port_base::parity::none));
    m_serial.set_option(asio::serial_port_base::stop_bits(asio::serial_port_base::stop_bits::one));
    m_serial.set_option(asio::serial_port_base::flow_control(asio::serial_port_base::flow_control::none));

    if (ec) {
        std::cerr << "[MAX232] Failed to configure port options: " << ec.message() << std::endl;
        m_serial.close(ec); // Clean up on fail
        return false;
    }
    m_connected = true;

    // Kick off the background listening loop
    start_async_read();

    // Spawn a dedicated thread to process ASIO events without blocking the 6502
    m_io_thread = std::thread([this]() { 
        m_io_context.restart();
        m_io_context.run(); 
    });
    
    std::cout << "[MAX232] Link established on " << port_name << " at " << baud_rate << " baud.\n";
    return true;
}

void Serial_Port::disconnect() {
    if (!m_connected) return;

    m_connected = false;

    if (m_serial.is_open()){
        m_serial.cancel();  // Stop pending async operations
        m_serial.close();
    }

    m_io_context.stop();

    if (m_io_thread.joinable()){
        m_io_thread.join();
    }

    std::cout << "[MAX232] Disconnected." << std::endl;
}

void Serial_Port::log_char(u8 c) {
    // Only log printable ASCII characters and newlines
    if ((c >= 32 && c <= 126) || c == '\n' || c == '\r'){
        m_terminal_log += (char)c;
    } else {
        m_terminal_log += "."; // Replace uprintable hex with a dot
    }

    // Keep the buffer from growing forever and eating all the RAM
    if (m_terminal_log.size() > 4096) {
        m_terminal_log.erase(0, 1024);
    }
}

// The Tx path (CPU -> ACIA -> MAX232 -> Real World)
void Serial_Port::update() {
    if (!m_connected) return;

    // Drain the ACIA's transmit queue
    while (m_acia->has_tx_data()){
        u8 tx_byte = m_acia->pop_tx_data();
        asio::error_code ec;
        
        // Synchronous write is fine here since the OS buffers it immediately
        // This represents T1IN to T1OUT on U8
        asio::write(m_serial, asio::buffer(&tx_byte, 1));
        if (!ec) {
            m_tx_bytes++;
            log_char(tx_byte);
        } else {
            std::cerr << "[MAX232] Transmit error: " << ec.message() << std::endl;
            disconnect();
            break;
        }
    }
}

// The Rx Path (Real World -> MAX232 -> ACIA -> CPU)
void Serial_Port::start_async_read() {
    if (!m_connected || !m_serial.is_open()) return;

    // This represents R1IN to R1OUT on U8
    m_serial.async_read_some(asio::buffer(&m_rx_buffer, 1),
        [this](const asio::error_code& error, std::size_t bytes_transferred) {
            if (!error && bytes_transferred > 0) {

                m_rx_bytes++;
                log_char(m_rx_buffer);
                // pass the raw byte directly to the ACIA (U7)
                m_acia->rx_char(m_rx_buffer);

                // Instanly re-arm the interrupt to listen for the next byte
                start_async_read();
            } else if (error != asio::error::operation_aborted){
                std::cerr << "[MAX232] Receive error: " << error.message() << std::endl;
                disconnect();
            }
        });
}