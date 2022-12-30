# HomeDMX

HomeKit Bridge Accesory built on top of ESP32 [HomeSpan](https://github.com/HomeSpan/HomeSpan) to controll DMX512 Light fixtures. Tested on Sparkfun ESP32 Thing Plus with Sparkfun DMX to LED Shield

## Description

I installed some moving heads and DMX-controlled LED dimmers in my daughter's room a few years ago. Recently, she expressed a desire to be able to trigger different lighting scenes using Siri on her HomePod, so I purchased an ESP32 microcontroller with a DMX 512 shield to enable this functionality. Now, she can simply say "Hey Siri, turn on Scene X" and the lighting in her room will change to the specified scene.

The project use VSCode editor, Arduino frameworks and ESP32 toolchain for Arduino   

## Getting Started

### Dependencies

* [HomeSpan](https://github.com/HomeSpan/HomeSpan)
* [SparkFunDMX](https://github.com/sparkfun/SparkFunDMX)

### Installing dependecies

* **[Installing an Arduino Library Guide](https://learn.sparkfun.com/tutorials/installing-an-arduino-library)** - Basic information on how to install an Arduino library.

### Installing
* Install Arduino, ESP32 Toolchain, Dependencies, VSCode and Arduino Extension
* Open folder in VSCode
* Press F1 -> Select Arduino: Upload

## Authors

Contributors names and contact info

Harmoniq Punk  
[Harmoniq Punk](https://github.com/harmoniqpunk)

## Version History

* 0.1.0
    * Initial Release

## License

This project is licensed under the MIT License - see the LICENSE.md file for details

## Acknowledgments

Inspiration, code snippets, etc.
* [homespan-examples-bridges](https://github.com/HomeSpan/HomeSpan/blob/master/examples/08-Bridges/08-Bridges.ino)