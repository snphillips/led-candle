# LED Proximity Sensing Flickering Candle

An arduino led candle that flickers when something goes near it. This project is inspired by My [New Flame](https://www.ingo-maurer.com/en/products/my-new-flame/) by Moritz Waldemeyer for Ingo Maurer. Technically, this project is adapted from Phillip Burgess/paintyourdragon's [Animated Flame Pendant](https://learn.adafruit.com/animated-flame-pendant).
  
  <video width="200" src="https://user-images.githubusercontent.com/36208853/235734782-392fa293-edab-47d8-b8b2-89ca9e333de9.mov"></video>




## Prerequisites

- Read through Phillip Burgess' Animated Flame Pendant [Animated Flame Pendant docs](https://learn.adafruit.com/animated-flame-pendant).
- Review the [Adafruit Time of Flight Distance Sensor docs](https://learn.adafruit.com/adafruit-vl53l0x-micro-lidar-distance-sensor-breakout)
- Have the [Arduino IDE](https://www.arduino.cc/en/software) installed on your computer
- Have the parts listed in the Parts section below, plus wire, solder, flux & soldering iron
- If you want to generate your own animations, have Python & FFmpeg installed on your computer (the leading multimedia framework): https://ffmpeg.org/

## How to Install Software onto the Adafruit ItsyBitsy MO Express

First, install the [Arduino IDE](https://www.arduino.cc/en/software).

- Clone this repo:

`git clone https://github.com/snphillips/led-candle.git`

- Connect the Adafruit ItsyBitsy MO Express into your computer via USB (ensure it's a USB data cable).

- Using the Arduino IDE, open `led-candle.ino`.

- Select Adafruit ItsyBitsy MO Express from the Tools->Board menu.
  Next go into the Tools -> Programmer menu and select the USBtinyISP programmer.

- Upload `led-candle.ino` to the Adafruit ItsyBitsy board by pressing the tiny button on the Adafruit ItsyBitsy. Within eight seconds, select Sketch > Upload Using Programmer. Wait until you see the message "Done uploading".
  If your code uploaded successfully, at the end of the output message you'll see a paragraph like this:

`Verify successful
Done in 3.607 seconds
writeWord(addr=0xe000ed0c,value=0x5fa0004)`

Nothing will happen yet, but now the code is on your Adafruit ItsyBitsy.

## How to Assemble Hardware

TODO: flush out this section

## How to Generate Your Own Animations

This project requires two short animations. If you don't like mine you can make your own.

You'll need [python](https://www.python.org/about/gettingstarted/) & [FFmpeg](https://ffmpeg.org/) installed on your computer.

- Take two vertical videos with you phone then airdrop them it to your computer.
- Edit the video to your liking. I use Apples's iMovie for this step. Remove the sound track, adjust contrast, make it greyscale, crop it. Keep in mind we have limited space on the small Adafruit ItsyBitsy. Your final animations can't be longer than about 15 seconds total.
- Export the two source videos you'd like to turn into animations. Use a low resolution as you'll eventually be resizing each frame to 9px x 16px.
- Place each video in their own folders named **flame-normal-source-mp4** and **flame-flicker-source-mp4**
- Name the normal flame video `flame-normal-source.mp4`, and name the flicker flame video `flame-flicker-source.mp4`
- We're going to use `FFmpeg` via the command line to create PNGs.
- We'll start with the flicker video. Navigate into the **flame-flicker-source-mp4** folder:

```
cd flame-flicker-source-mp4
```

- Convert the video to a suite of PNGs. Every second of video will generate 30 PNGs (30 frames/second). This command will extract frames at a rate of 30 FPS, resize them to 9x16 pixels, and save them as PNG files with a 3-digit frame number in the output directory. The %03d in the output file name is a placeholder for the frame number, which will be zero-padded to 3 digits (e.g., 001, 002, 003, etc.).

```
 ffmpeg -i flame-flicker-source.mp4 -vf "fps=30,scale=9:16" flame-flicker-source-pngs/flicker-frame_%03d.png
```

- Navigate into the **flame-flicker-source-pngs** folder to confirm you see a bunch of PNGs that are 9x16.

```
cd flame-flicker-source-pngs
```

- Run the following python script to generate an `h` file. After you've run the script, confirm you see a file called `animationFlicker.h` in the folder.

```
python3 convert-flame-flicker.py *.png > animationFlicker.h
```

- Move the `h` file `animationFlicker.h` into the root of the project folder: led-candle.

- Repeat the above steps for the normal video **flame-normal-source.mp4**.

```
 ffmpeg -i flame-normal-source.mp4 -vf "fps=30,scale=9:16" flame-normal-source-pngs/normal-frame_%03d.png
```

- Navigate into the **flame-normal-source-pngs** folder then run the following python script to generate an `h` file. After you've run the script, if you see a file called `animationNormal.h` in the folder, the script worked.

```
cd flame-normal-source-pngs
```

```
python3 convert-flame-normal.py *.png > animationNormal.h
```

- Move the `h` file `animationFlicker.h` into the root of the project folder: led-candle.

- Confirm that the project folder contains the three files needed to upload to the Adafruit Adafruit ItsyBitsy MO Express: `led-candle.ino`, `animationNormal.h`, `animationFlicker.h`.

- Connect the Adafruit ItsyBitsy MO Express into your computer. The instructions below assume you connected it using USB.

- Using the Arduino IDE, open `led-candle.ino`.

- Upload `led-candle.ino`, `animationNormal.h` & `animationFlicker.h` to the Adafruit ItsyBitsy MO Express board by pressing the tiny button on the Adafruit ItsyBitsy MO Express. Within eight seconds, select Sketch > Upload Using Programmer. Wait until you see the message "Done uploading".

## Parts

- Adafruit ItsyBitsy MO Express microcontroller [3727](https://www.adafruit.com/product/3727)
- Charlieplex LED Matrix Driver (Adafruit product id: [2946](https://www.adafruit.com/product/2946))
- Charlieplex LED Matrix (Adafruit product id: 2947, [2948](https://www.adafruit.com/product/2948), 2972, 2973 or 2974)
- 350 mAh LiPoly battery (Adafruit product id: [2750](https://www.adafruit.com/product/2750))
- LiPoly backpack (Adafruit product id: [2124](https://www.adafruit.com/product/2224))
- On-Off Power Button / Pushbutton Toggle Switch (Adafruit product id: [1683](https://www.adafruit.com/product/1683))
- Adafruit VL53LOX Time of Flight Distance Sensor (Adafruit product id: [3317](https://www.adafruit.com/product/3317))
