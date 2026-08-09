#pragma once
#include <cstdint>

inline uint64_t read_u64_be(const uint8_t* p){
    uint64_t v = 0;
    for (int i = 0; i < 8; i++){
        v = (v << 8) | p[i];
    };
    return v;
} 

inline uint64_t read_u48_be(const uint8_t* p){
    uint64_t v = 0;
    for (int i = 0; i < 6; i++){
        v = (v << 8) | p[i];
    };
    return v;
}

inline uint32_t read_u32_be(const uint8_t* p){
    uint32_t v = 0;
    for (int i = 0; i < 4; i++){
        v = (v << 8) | p[i];
    };
    return v;
}

inline uint16_t read_u16_be(const uint8_t* p){
    uint16_t v = 0;
    for (int i = 0; i < 2; i++){
        v = (v << 8) | p[i];
    };
    return v;
}