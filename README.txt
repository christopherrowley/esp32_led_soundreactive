NOTE: I am by no means an expert in electronics, or in using micro-controllers. I just wanted some cool sound reactive lights for my apartment and brute forced my way through it. I am sharing my notes to give people a good starting point to work from. That being said, I'd love to hear any feedback on how this could be improved!

The starting point for the project, and the place where most of the code came from is: https://www.youtube.com/watch?v=yninmUrl4C0

The main difference between this project and mine are:
- switch to esp32 board instead of esp8266
- no LED mode selection
- bluetooth sound input instead of a microphone (important because I can't have music cranked in my apartment, so this will work while wearing headphones!)
- fast fourier transform (FFT) library included to react to spectral content and not volume

The code for the last point was hijacked from the arduino project here:
https://create.arduino.cc/projecthub/Shajeeb/32-band-audio-spectrum-visualizer-analyzer-902f51

This also relies on a way to have your audio split so you can hear it and "see" it. Newer android phones have native support to broadcast audio signal to two devices, so this solved the problem for me.

Overall this project took way longer than I thought it would, and hopefully this info will help the next person have a running head start. I went with the esp32 board thinking that I could use the bluetooth capabilities of the chip to receive the audio signal. After some reading, I think the chip uses the same antenna for WIFI and bluetooth, so I don't think it can do both simultaneously as I was hoping. Also, I wasn't able to figure out the bluetooth arduino a2dp code to get it to read in music from my phone. So I ended up cheating and just buying an audio bluetooth receiver for cheap to get the audio signal to the board.

The basic breakdown is that the master LED contains the bluetooth module, inputs the audio, recenters it, performs an FFT, scales the value, then sends to the slave LED controllers via the wifi network it generates. Each LED then processes that value to determine how to adjust the lighting on the LED strip.

I use a DC offset for the audio signal, which should center the value on 512, except I am getting it centered around 1420 (see CENTER_VAL in master code). I didn't have time to figure out what's going on, so if you get bored, you can do that! Anyway, this seems to work by just changing the center_val to what it looks like.

Also I have never had any training on circuit diagrams. I made the ones included to help me remember what goes where. Should be easy enough to figure out what to do from them.

What you will need for X number of LED lamps is:
X - esp32 controller boards
X - perf boards to solder your wires up.
X - ws2812b LED strips (I used 1m strand with 60 lights, make sure the strips you get are rated for 5V!)
X - 330 Ohm resistors (for filtering noise to LEDs from controller)
X - 100 microFarad (uF) capacitors
X - 5V DC power supplies
2 - 100k Ohm resistors (DC bias input audio signal)
1 - bluetooth sound receiver

and then anything else you want to style your lamps! (watch the first video link for how-to guide on that)
