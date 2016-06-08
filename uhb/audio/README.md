# PyAudio based raw audio capture/playback

`capture.py` : records audio from a user specified microphone (or system
               default) and stores the raw data in a GDP log
`playback.py`: given a GDP log with raw audio data produced by `capture.py`,
               it plays it on the default speaker


# Software/hardware requirements

## Operating System

Tested on Linux and it works. It may work on a Mac, but not yet tested. If
pyAudio works on Mac (which it probably should, since it's based on PortAudio
whose selling point is being cross-platform).

## pyaudio and ALSA

It works with recent-ish versions of PyAudio. Use `pip install pyaudio` to get
the latest version if you are unsure. (I had to do this on a BeagleBone with
debian, because the repository package is too old). In addition, ALSA can be
useful to debug if things don't work well.

## Hardware

Of course, you need a microphone/speaker for recording/playback. However, the
capabilities vary across hardware. For example, I have tested the following:

- Laptop's mic: 2 Channel, default sampling @ 44.1KHz
- USB dongle Microphone: 1 Channel, default sampling @ 44.1KHz
- PlayStation Eye Camera: 4 Channel, default sampling @ 16KHz

The quality of sound (volume, to be more precise) varies quite a bit across
different hardware as well. In theory, it should be possible to filter/adjust
this by processing the raw data. However, I have not tested if that's true.
In case you have multiple devices connected at the same time, you can specify
which one to use by specifying a device name prefix (see below).


# Execution and data-format

`capture.py`: Takes the name of a pre-existing log to which to write data to,
and an optional device prefix (useful if you have multiple microphones). It
automatically queries the device for the channel/sampling information and
writes that as a JSON object in the first record of the log. Hence, reusing
the same log for different devices will not work well. A configurable
parameter (depending on your proximity to a GDP server) is the number of
records that should be written to the log per second. Recommended range for
this value is 1-10.

`playback.py`: This takes the name of a log as the only parameter. It reads
the first record to get the JSON encoded stream parameters, and sets up the
playback appropriately. By default, it plays back the audio indefinitely
with a slight delay; the amount of this delay can be configured by DELAY_RECS

