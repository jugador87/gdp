#!/usr/bin/env python

""" Subscribe to a log and play audio data """

import gdp
import sys
import time
import json
import pyaudio
from threading import Lock


DELAY_RECS = 5


pcm_buf = ""
channels = None
sampleWidth = None
samplingRate = None


def playAudio(lh):

    print ">>>", "sampleWidth:", sampleWidth, "channels:", channels,
    print "samplingRate:", samplingRate

    def __helperCallback(in_data, frame_count, time_info, status):
        global pcm_buf
        ret_len = frame_count*channels*sampleWidth
        while len(pcm_buf)<ret_len:
            time.sleep(0.01)
        pcm_buf_lock.acquire()
        out_data = pcm_buf[:ret_len]
        pcm_buf = pcm_buf[ret_len:]
        pcm_buf_lock.release()
        return (out_data, pyaudio.paContinue)

    p = pyaudio.PyAudio()
    stream = p.open(format=p.get_format_from_width(sampleWidth),
                    channels=channels, rate=samplingRate,
                    input=False, output=True, stream_callback=__helperCallback)

    global pcm_buf
    pcm_buf_lock = Lock()
    # subscribe to the log
    lh.subscribe(-DELAY_RECS, 0, None)
    stream.start_stream()
    
    while True:
        event = gdp.GDP_GCL.get_next_event(None)
        pcm_buf_lock.acquire()
        pcm_buf += event['datum']['data']
        pcm_buf_lock.release()
    
    stream.stop_stream()
    stream.close()
    p.terminate()
    

def main(logname):

    gdp.gdp_init()
    lh = gdp.GDP_GCL(gdp.GDP_NAME(logname), gdp.GDP_MODE_RO)

    firstRecord = lh.read(1)
    audioParams = json.loads(firstRecord['data'])
    global sampleWidth
    global samplingRate
    global channels
    sampleWidth = audioParams['sampleWidth']
    samplingRate = audioParams['samplingRate']
    channels = audioParams['channels']

    playAudio(lh)


if __name__=="__main__":
    if len(sys.argv)<2:
        print "Usage: %s logname" % sys.argv[0]
        sys.exit(1)

    main(sys.argv[1])
