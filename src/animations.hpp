// Contains helper fonctions to fill the pixels with colors
// Header only because of templates
#pragma once

#include "NeoPixelBus.h"

template<typename T1, typename T2>
void rotateStrip(NeoPixelBus<T1, T2>& strip, const uint16_t& pixelCount) {
  auto firstColor = strip.GetPixelColor(0);
  for(uint16_t i = 0; i < pixelCount - 1; i++){
    strip.SwapPixelColor(i, i + 1);
  }
  strip.SetPixelColor(pixelCount - 1, firstColor);
}


template<typename T1, typename T2>
void rainbowFill(NeoPixelBus<T1, T2>& strip, const uint16_t& pixelCount){
  auto color = HsbColor(0, 1, 1);
  double currentHue = 0.0f;
  
  for(uint16_t i = 0; i < pixelCount; i++){
    strip.SetPixelColor(i, color);
    color.H = currentHue;
    
    currentHue += 1.0/(float)pixelCount;
    if(currentHue > 1.0){
      currentHue = 0;
    }
  }
}

template<typename T1, typename T2, typename Color>
void fillColor(NeoPixelBus<T1, T2>& strip, const uint16_t& pixelCount, const Color& color) {
    for(uint16_t i = 0; i < pixelCount; i++) {
        strip.SetPixelColor(i, color);
    }
}