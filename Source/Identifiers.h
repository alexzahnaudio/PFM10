//
//  Identifiers.h
//  PFM10 - Shared Code
//
//  Created by Alex Zahn on 10/6/23.
//  Copyright © 2023 Alex Zahn. All rights reserved.
//

#pragma once

namespace IDs
{
    #define DECLARE_ID(name) const juce::Identifier name (#name);

    DECLARE_ID (root)
    DECLARE_ID (thresholdValue)
    DECLARE_ID (decayRate)
    DECLARE_ID (averagerIntervals)
    DECLARE_ID (peakHoldEnabled)
    DECLARE_ID (peakHoldInf)
    DECLARE_ID (peakHoldDuration)
    DECLARE_ID (goniometerScale)

#undef DECLARE_ID

}
