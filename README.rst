moodpd
======

A simple daemon which listens to udp packets and controls color of connected `moodlamp <http://wiki.muc.ccc.de/moodlamp>`_. Runs on trieste at `sublab <https://sublab.org/>`_ Leipzig. 


Raw command packet format
-------------------------

Send UDP packets to port 4242 consisting of ASCII string 'm00d' immediately followed by one control command. 

Commands::

        #RRGGBB     set the color (https://en.wikipedia.org/wiki/Web_colors#Hex_triplet).

        !...        send raw command bytes to mood lamp (only if enabled)

        old muccc-style commands (compile-time option, currently disabled):
            BVV         sets global brightness to VV (CMD_SET_BRIGHTNESS)
            FRRGGBBTTTT fade to color RRGGBB in TTTT milliseconds (CMD_FADEMS)
            P           cycle pause state (CMD_PAUSE)
            X           CMD_POWER

Setting the color, in (pseudo-)C::

        int sock= socket(AF_INET, SOCK_DGRAM, 0);
        sprintf(buf, "m00d#%02X%02X%02X", red, green, blue);
        sendto(sock, buf, strlen(buf), address...);

Sending command packets from the command line, using `socat <http://www.dest-unreach.org/socat/>`_::

        $ socat - UDP4-DATAGRAM:localhost:4242,
        m00d#104080
        m00d#f0f0f0
        ...


`Open Sound Control <http://opensoundcontrol.org/>`_ interface
--------------------------------------------------------------

The daemon now understands OSC commands, so anything which can send configurable OSC messages via UDP can control the mood lamp (e.g. puredata). The OSC server listens on port 4243.

OSC message paths and arguments::

	/moodpd/lamps/00/rgb int32 int32 int32		Set color value of first connected lamp to given RGB values. Values will be clamped to range 0..255.
	/ori int32 int32 int32				Roll, yaw, pitch values sent py Android phone OSC app

Android orientation sensor
__________________________

moodpd now translates `andOSC <http://www.appbrain.com/app/andosc/cc.primevision.andosc>`_'s roll, yaw, pitch messages to moodlamp RGB values. The app has a configuration page, set IP address and port there and you can control the color of the lamp by a wave of your phone...



