// MIT License
//
// Copyright (c) 2020 Mechatronics and Haptic Interfaces Lab - Rice University
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// Author(s): Nathan Dunkelberger (nbd2@rice.edu)

#include <Windows.h>
#include <tchar.h>

#include <Mahi/Fes/Core/Event.hpp>
#include <Mahi/Fes/Core/Stimulator.hpp>
#include <Mahi/Fes/Utility/Communication.hpp>
#include <Mahi/Util.hpp>
#include <codecvt>
#include <locale>
#include <mutex>
#include <string>

using namespace mahi::util;

namespace mahi {
namespace fes {

Stimulator::Stimulator(const std::string& name_, std::vector<Channel>& channels_,  const std::string& com_port_1_, const std::string& com_port_2_, bool is_virtual_) :
    m_name(name_),
    m_com_port_1(com_port_1_),
    m_com_port_2(com_port_2_),
    m_com_ports({m_com_port_1, m_com_port_2}),
    m_hComms({&m_hComm_1, &m_hComm_2}),
    m_enabled(false),
    m_is_virtual(is_virtual_),
    m_channels(channels_),
    m_scheduler_1(),
    m_scheduler_2(),
    m_schedulers({&m_scheduler_1, &m_scheduler_2}),
    num_events(channels_.size()),
    amplitudes(num_events, 0),
    pulsewidths(num_events, 0),
    max_amplitudes(num_events, 0),
    max_pulsewidths(num_events, 0) {
    for (auto i = 0; i < num_events; i++) {
        max_amplitudes[i]  = m_channels[i].get_max_amplitude();
        max_pulsewidths[i] = m_channels[i].get_max_pulse_width();
        channel_names.push_back(m_channels[i].get_channel_name());
    }
    if (m_com_port_2.compare("NONE") != 0){
        m_num_ports = 2;
    }
    
    enable();
}

Stimulator::~Stimulator() { disable(); }

// Open and configure serial port, and initialize the channels on the board.
bool Stimulator::enable() {
    // open the comport with read/write permissions
    for (size_t i = 0; i < m_num_ports; i++)
    {
        if (!open_port(m_hComms[i], m_com_ports[i])) {
            disable();
            return m_enabled;
        }
        // Configure the parameters for serial port ttyUSB0
        if (!configure_port(m_hComms[i])) {
            disable();
            return m_enabled;
        }
    }
    // Write stim board setup commands to serial port ttyUSB0
    if (!initialize_board()) {
        disable();
        return m_enabled;
    }
    m_enabled = true;
    return m_enabled;
}

void Stimulator::disable() {
    if (is_enabled()) {
        for (size_t i = 0; i < m_num_ports; i++){
            m_schedulers[i]->disable();
        }
        close_stimulator();
        LOG(Info) << "Stimulator Disabled";
    } else {
        LOG(Info) << "Stimulator has not been enabled yet.";
    }
    m_enabled = false;
}

bool Stimulator::open_port(HANDLE* hComm, std::string com_port) {
    // the comport must be formatted as an LPCWSTR, so we need to get it into that form from a
    // std::string
    std::wstring com_prefix = L"\\\\.\\";
    std::wstring com_suffix =
        std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(com_port);
    std::wstring comID = com_prefix + com_suffix;

    // std::wstring stemp = std::wstring(com_port_formatted.begin(), com_port_formatted.end());
    // LPCWSTR com_port_lpcwstr = stemp.c_str();

    *hComm = CreateFileW(comID.c_str(),                 // port name
                        GENERIC_READ | GENERIC_WRITE,  // Read/Write
                        0,                             // No Sharing
                        NULL,                          // No Security
                        OPEN_EXISTING,                 // Open existing port only
                        0,                             // Non Overlapped I/O
                        NULL);                         // Null for Comm Devices

    // Check if creating the comport was successful or not and log it
    if (*hComm == INVALID_HANDLE_VALUE) {
        LOG(Error) << "Failed to open port " << com_port;
        return false;
    } else {
        LOG(Info) << "Successfully opened port " << com_port;
    }

    return true;
}

bool Stimulator::configure_port(HANDLE* hComm) {  // configure_port establishes the settings for each serial port

    // http://bd.eduweb.hhs.nl/micprg/pdf/serial-win.pdf

    m_dcbSerialParams.DCBlength = sizeof(DCB);

    if (!GetCommState(*hComm, &m_dcbSerialParams)) {
        LOG(Error) << "Error getting serial port state";
        return false;
    }

    // set parameters to use for serial communication

    // set the baud rate that we will communicate at to 9600
    m_dcbSerialParams.BaudRate = CBR_9600;

    // 8 bits in the bytes transmitted and received.
    m_dcbSerialParams.ByteSize = 8;

    // Specify that we are using one stop bit
    m_dcbSerialParams.StopBits = ONESTOPBIT;

    // Specify that we are using no parity
    m_dcbSerialParams.Parity = NOPARITY;

    // Disable all parameters dealing with flow control
    m_dcbSerialParams.fOutX       = FALSE;
    m_dcbSerialParams.fInX        = FALSE;
    m_dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
    m_dcbSerialParams.fDtrControl = DTR_CONTROL_DISABLE;
    // dcbSerialParams.fOutxCtsFlow = FALSE;
    // dcbSerialParams.fOutxDsrFlow = FALSE;

    // Set communication parameters for the serial port
    if (!SetCommState(*hComm, &m_dcbSerialParams)) {
        LOG(Error) << "Error setting serial port state";
        return false;
    }

    COMMTIMEOUTS timeouts                = {0};
    timeouts.ReadIntervalTimeout         = 10;
    timeouts.ReadTotalTimeoutConstant    = 10;
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(*hComm, &timeouts)) {
        LOG(Error) << "Error setting serial port timeouts";
        return false;
    }

    PurgeComm(*hComm,PURGE_TXABORT);
    PurgeComm(*hComm,PURGE_RXABORT);
    PurgeComm(*hComm,PURGE_RXCLEAR);
	PurgeComm(*hComm,PURGE_TXCLEAR);

    return true;
}

bool Stimulator::initialize_board() {
    // delay time after sending setup messages of serial comm

    for (auto i = 0; i < m_channels.size(); i++) {
        if (!m_channels[i].setup_channel(*m_hComms[m_channels[i].get_board_num()], m_delay_time)) {
            return false;
        };
    }

    LOG(Info) << "Setup Completed successfully.";

    return true;
}

bool Stimulator::halt_scheduler() { 
    bool success = true;
    for (size_t i = 0; i < m_num_ports; i++)
    {
        if(!m_schedulers[i]->halt_scheduler()){
            success = false;
        }
    }
    return success;
}

void Stimulator::close_stimulator() {
    for (size_t i = 0; i < m_num_ports; i++){
        CloseHandle(*m_hComms[i]);
    }
    
    m_enabled = false;
}

bool Stimulator::begin() {
    if (is_enabled()) {
        m_enabled = true;
        bool success = true;
        for (size_t i = 0; i < m_num_ports; i++)
        {
            if(!m_schedulers[i]->send_sync_msg()){
                success = false;
            }
        }
        return success;
    } else {
        LOG(Error) << "Stimulator has not yet been opened. Not starting the stimulator";
        return false;
    }
}

void Stimulator::set_amp(Channel channel_, unsigned int amp_) {
    if (is_enabled()) {
        m_schedulers[channel_.get_board_num()]->set_amp(channel_, amp_);
    } else {
        LOG(Error) << "Stimulator has not yet been enabled. Not writing amplitude";
    }
}

void Stimulator::set_amps(std::vector<Channel> channels_, std::vector<unsigned int> amplitudes_) {
    for (size_t i = 0; i < channels_.size(); i++) {
        set_amp(channels_[i], amplitudes_[i]);
    }
}

void Stimulator::write_pws(std::vector<Channel> channels_, std::vector<unsigned int> pulsewidths_) {
    for (size_t i = 0; i < channels_.size(); i++) {
        write_pw(channels_[i], pulsewidths_[i]);
    }
}

void Stimulator::write_pw(Channel channel_, unsigned int pw_) {
    if (is_enabled()) {
        m_schedulers[channel_.get_board_num()]->write_pw(channel_, pw_);
    } else {
        LOG(Error) << "Stimulator has not yet been enabled. Not writing pulsewidth";
    }
}

void Stimulator::update_max_amp(Channel channel_, unsigned int max_amp_) {
    for (auto channel = m_channels.begin(); channel != m_channels.end(); channel++) {
        // if the channel is the correct channel we are looking for, set the max amplitude of the
        // channel
        if (channel->get_channel_name() == channel_.get_channel_name()) {
            channel->set_max_amplitude(max_amp_);
            return;
        }
    }
    LOG(Error) << "Did not find the correct channel to update";
}

void Stimulator::update_max_pw(Channel channel_, unsigned int max_pw_) {
    for (auto channel = m_channels.begin(); channel != m_channels.end(); channel++) {
        // if the channel is the correct channel we are looking for, set the max pulsewidth of the
        // channel
        if (channel->get_channel_name() == channel_.get_channel_name()) {
            channel->set_max_pulse_width(max_pw_);
            return;
        }
    }
    LOG(Error) << "Did not find the correct channel to update";
}

bool Stimulator::update() {
    if (is_enabled()) {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            for (size_t j = 0; j < m_num_ports; j++)
            {
                for (size_t i = 0; i < m_schedulers[j]->get_num_events(); i++) {
                    Channel channel_ = m_schedulers[j]->get_events()[i].get_channel();
                    
                    amplitudes[channel_.get_channel_num()]      = m_schedulers[j]->get_amp(channel_);
                    pulsewidths[channel_.get_channel_num()]     = m_schedulers[j]->get_pw(channel_);
                    max_amplitudes[channel_.get_channel_num()]  = channel_.get_max_amplitude();
                    max_pulsewidths[channel_.get_channel_num()] = channel_.get_max_pulse_width();
                }
            }
        }
        bool success = true;
        for (size_t i = 0; i < m_num_ports; i++)
        {
            if(!m_schedulers[i]->update()){
                success = false;
            }
        }
        std::vector<ReadMessage> incoming_messages = get_all_messages(m_hComms,m_num_ports);
        for (size_t i = 0; i < incoming_messages.size(); i++){
            if (!incoming_messages[i].is_valid()){
                LOG(Error) << "Return message (below) either invalid or an error. Disabling stimulator.";
                print_message(incoming_messages[i].get_message());
                success = false;
            }
        }
        if (!success) disable();
        return success;
    } else {
        LOG(Error) << "Stimulator has not yet been enabled. Not updating";
        return false;
    }
}

bool Stimulator::create_scheduler(const unsigned char sync_msg, double frequency_) {
    unsigned int duration;
    if (frequency_ > 0) {
        duration = (unsigned int)(1.0 / frequency_ * 1000);
    } else {
        duration = 50;
    }

    if (is_enabled()) {
        bool success = false;
        for (size_t i = 0; i < m_num_ports; i++){
            bool success = m_schedulers[i]->create_scheduler(*m_hComms[i], sync_msg, duration, m_delay_time);
            if (!m_is_virtual){
                ReadMessage scheduler_created_msg(read_message(*m_hComms[i], true));
                if (scheduler_created_msg.is_valid()){
                    m_schedulers[i]->set_id(scheduler_created_msg.get_data()[0]);
                }
                else{
                    LOG(Error) << "Scheduler created return message (below) was either invalid or an error. Disabling stimulator.";
                    print_message(scheduler_created_msg.get_message());
                    disable();
                    return false;
                }
            }
        }
        return success;
    } else {
        LOG(Error) << "Stimulator has not yet been enabled. Not creating scheduler";
        return false;
    }
}

bool Stimulator::add_event(Channel channel_, unsigned char event_type) {
    if (is_enabled()) {
        bool success = m_schedulers[channel_.get_board_num()]->add_event(channel_, m_delay_time, event_type);
        return success;
    } else {
        LOG(Error) << "Stimulator has not yet been enabled. Not adding event to scheduler";
        return false;
    }
}

bool Stimulator::add_events(std::vector<Channel> channels_, unsigned char event_type) {
    if (is_enabled()) {
        for (size_t i = 0; i < channels_.size(); i++) {
            // If any channel fails to add, return false after throwing an error
            if (!add_event(channels_[i], event_type)) {
                return false;
            };
        }
        return true;
    } else {
        LOG(Error) << "Stimulator has not yet been enabled. Not adding event to scheduler";
        return false;
    }
}

std::vector<Channel> Stimulator::get_channels() { return m_channels; }

bool Stimulator::is_enabled() { return m_enabled; }

std::string Stimulator::get_name() { return m_name; }

// void Stimulator::read_all() {
//     DWORD         msg_size = 1;
//     unsigned char msg[1];
//     bool          done       = false;
//     bool          first_pass = true;

//     DWORD dwBytesRead = 0;
//     while (!done) {
//         if (!ReadFile(m_hComm, msg, msg_size, &dwBytesRead, NULL)) {
//             LOG(Error) << "Could not read message";
//         }
//         if (dwBytesRead != 0) {
//             if (first_pass) {
//                 std::cout << "Message: ";
//                 first_pass = false;
//             }
//             char char_buff[20];
//             sprintf(char_buff, "0x%02X", (unsigned int)msg[0]);
//             std::cout << char_buff << ", ";
//         } else {
//             std::cout << std::endl;
//             done = true;
//         }
//     }
// }
} // namespace fes
} // namespace mahi
