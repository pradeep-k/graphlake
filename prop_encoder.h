#pragma once

#include "type.h"

class prop_encoder_t {
    public:
    virtual status_t encode(const char* str_time, univ_t& value) = 0;
    virtual void print(univ_t value) = 0;

};

class time_encoder_t : public prop_encoder_t {
    public:
    status_t encode(const char* str_time, univ_t& value);
    void print(univ_t value);

    //Add filters like <, > etc
};

class int64_encoder_t : public prop_encoder_t {
 public:
    status_t encode(const char* str, univ_t& value);
    void print(univ_t value);
};

class embedstr_encoder_t : public prop_encoder_t {
 public:
    status_t encode(const char* str, univ_t& value);
    void print(univ_t value);
}; 
