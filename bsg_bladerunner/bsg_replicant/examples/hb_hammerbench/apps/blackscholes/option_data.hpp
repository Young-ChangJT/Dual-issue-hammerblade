#pragma once
typedef struct OptionData_ {
        // Inputs
        float s;          // spot price
        float strike;     // strike price
        float r;          // risk-free interest rate
        float v;          // volatility
        float t;          // time to maturity or option expiration in years 
        // Outputs;
        float call;
        float put;
        float unused;
} OptionData;
