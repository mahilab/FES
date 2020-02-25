#pragma once

// #include "FES/Core/Stimulator.hpp"
#include "FES/Core/Channel.hpp"

namespace fes{
    class Event{
    private:
        #define DELETE_EVENT_LEN          0x01
        #define CHANGE_EVENT_PARAMS_LEN   0x04
        #define STIM_EVENT                0x03
        
        HANDLE hComm;
        unsigned char schedule_id;
        int delay_time;
        Channel channel;
        unsigned int pulse_width;
        unsigned int amplitude;
        unsigned char event_type;
        unsigned char priority;
        unsigned char zone;
        unsigned char event_id;
        unsigned int max_amplitude;
        unsigned int max_pulse_width;
    public:

        // Event();

        Event(HANDLE& hComm, unsigned char schedule_id_, int delay_time_, Channel channel_, unsigned char event_id_, int pulse_width_ = 0, int amplitude_ = 0, unsigned char event_type_ = STIM_EVENT, unsigned char priority_ = 0x00, unsigned char zone_ = 0x00);

        ~Event();
        bool create_event();

        unsigned char get_channel_num();
        
        std::string get_channel_name();

        bool edit_event();

        bool delete_event();

        bool update();

        void set_amplitude(unsigned int amplitude_);

        void set_pulsewidth(unsigned int pulse_width_);

        int get_amplitude();

        int get_pulsewidth();
    };
}



