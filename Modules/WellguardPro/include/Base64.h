/*
  Base64.h - Library for Base64 encoding and decoding on Arduino.
  Created by Adrien Grellard, April 20, 2017.
*/

#ifndef BASE64_H
#define BASE64_H

class Base64 {
public:
    static String encode(const byte* data, size_t size);
    static String decode(const String& data);
};

#endif
