//
//  DefaultPropertyValues.h
//  PFM10 - Shared Code
//
//  Created by Alex Zahn on 10/6/23.
//  Copyright © 2023 Alex Zahn. All rights reserved.
//

#pragma once

struct DefaultPropertyValues
{
    static constexpr float thresholdValue    = 0.0f;
    static const int       decayRate         = 12;
    static const int       averagerIntervals = 6;
    static const bool      peakHoldEnabled   = true;
    static const bool      peakHoldInf       = false;
    static const int       peakHoldDuration  = 500;
    static constexpr float goniometerScale   = 1.0f;
};
