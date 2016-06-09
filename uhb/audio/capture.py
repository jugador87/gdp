#!/usr/bin/env python

""" Capture audio data and write to a log in a non-blocking manner.

Requires a recent-ish version of PyAudio, tested on a Ubuntu (laptop) and
debian (beaglebone). See README for more information
"""

import gdp
import sys
import time
import json  # this is how we store data in the first record
import pyaudio

NUM_RECS_PER_SEC = 5


def getDeviceParams(inputDevicePrefix=None):
    """ returns the device ID and other parameters for the mic"""

    p = pyaudio.PyAudio()
    mic_id = None

    num_devices = p.get_device_count()
    for i in xrange(0, num_devices):
        dev_info = p.get_device_info_by_index(i)
        if dev_info.get('maxInputChannels')>0:
    	    # in case we don't want default microphone?
            if inputDevicePrefix is not None: 
                if dev_info.get('name').startswith(inputDevicePrefix):
                    mic_id = i
                    break
            else:
                mic_id = i
                break

    params = {}
    params['sampleWidth'] = 2     # Quite arbitrary
    params['samplingRate'] = int(p.get_device_info_by_index(mic_id).\
                                get('defaultSampleRate'))
    params['channels'] = int(p.get_device_info_by_index(mic_id).\
                                get('maxInputChannels'))
    params['samplesPerRecord'] = params['samplingRate']/NUM_RECS_PER_SEC

    p.terminate()

    return (mic_id, params)



def recordToLog(logHandle, sampleWidth=2, channels=1,
                            samplingRate=44100, samplesPerRecord=8820,
                            device_id=0):
    """
    Append raw audio data to a GDP 'logHandle'. Captured audio data from
        either the default input device, or a device that starts with a
        certain prefix (useful in case of multiple input devices).
    """

    print ">>>", "sampleWidth:", sampleWidth, "channels:", channels,
    print "samplingRate:", samplingRate, "samplesPerRecord", samplesPerRecord,
    print "device_id:", device_id

    def __helperCallback(in_data, frame_count, time_info, status):
        "callback that the stream calls when more data is available" 
        logHandle.append( {"data": in_data} )
        return (None, pyaudio.paContinue)

    p = pyaudio.PyAudio()
    stream = p.open(format=p.get_format_from_width(sampleWidth),
                        channels=channels, rate=samplingRate,
                        input=True, output=False,
                        input_device_index=device_id,
                        frames_per_buffer=samplesPerRecord,
                        stream_callback=__helperCallback)

    stream.start_stream()

    # need this to keep the main thread from terminating
    # IF you want to record for a finite time rather than indefinite 
    #   recording, this is where that logic would go.
    while stream.is_active():
        time.sleep(0.1)

    # cleanup. Although pointless in case of indefinite streaming
    stream.stop_stream()
    stream.close()
    p.terminate()


def main(logname, inputDevicePrefix=None):

    gdp.gdp_init()
    lh = gdp.GDP_GCL(gdp.GDP_NAME(logname), gdp.GDP_MODE_RA)

    # Okay, get the parameters
    try:
        # try reading from the first record
        firstRecord = lh.read(1)
        audioParams = json.loads(firstRecord['data'])
        mic_id, max_params = getDeviceParams(inputDevicePrefix)
        assert audioParams['samplingRate'] <= max_params['samplingRate']
        assert audioParams['channels'] <= max_params['channels']

    except gdp.MISC.EP_STAT_SEV_ERROR as e:
        # in case first record does not exist, let's write it
        if e.msg.startswith("ERROR: 404 ") or \
                        e.msg.startswith('ERROR: 4.04 '):
            mic_id, audioParams = getDeviceParams(inputDevicePrefix)
            lh.append( {"data": json.dumps(audioParams)} )
        else:
            # this is some other error, let's just raise it as it is
            raise e

    # start recording
    recordToLog(lh, sampleWidth=audioParams['sampleWidth'],
                    channels=audioParams['channels'],
                    samplingRate=audioParams['samplingRate'],
                    samplesPerRecord=audioParams['samplesPerRecord'],
                    device_id=mic_id)


if __name__=="__main__":

    if len(sys.argv)<2:
        print "Usage: %s LOGNAME [inputDevicePrefix]" % sys.argv[0]
        sys.exit(1)

    if len(sys.argv)==2:
        main(sys.argv[1])
    else:
        main(sys.argv[1], sys.argv[2])
