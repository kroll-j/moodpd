#!/usr/bin/env python
# moodlamp-farbe setzen ueber open sound control mit gtk colorchooser
# jkroll bei lavabit punkt com

import os
import sys
import time
import pygtk
pygtk.require('2.0')
import gtk
import liblo

class gui:
    def delete_event(self, widget, event, data=None):
        # Change FALSE to TRUE and the main window will not be destroyed
        # with a "delete_event".
        return False

    def destroy(self, widget, data=None):
        # signal main loop to quit.
        self.running= False

    def __init__(self):
        # create a new window
        self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
        
        # self.window.set_opacity(0.8)
        
        # self.window.modify_bg(gtk.STATE_NORMAL, self.window.get_colormap().alloc_color("#204080"))
        
        self.window.resize(700, 200)
    
        # When the window is given the "delete_event" signal (this is given
        # by the window manager, usually by the "close" option, or on the
        # titlebar), we ask it to call the delete_event () function
        # as defined above. The data passed to the callback
        # function is NULL and is ignored in the callback function.
        self.window.connect("delete_event", self.delete_event)
    
        # Here we connect the "destroy" event to a signal handler.  
        # This event occurs when we call gtk_widget_destroy() on the window,
        # or if we return FALSE in the "delete_event" callback.
        self.window.connect("destroy", self.destroy)
        
        # Sets the border width of the window.
        self.window.set_border_width(0)
    
        self.hbox= gtk.HBox()
        self.window.add(self.hbox)

        self.chooser= gtk.ColorSelection()
        self.chooser.connect("color-changed", self.color_changed, None)
        self.hbox.pack_start(self.chooser)
        
        self.vbox= gtk.VBox()
        self.hbox.pack_end(self.vbox)

        self.vbox.pack_start(gtk.Label("OSC URL"))
        self.oscurl_entry= gtk.Entry()
        self.vbox.pack_start(self.oscurl_entry)
        self.oscurl_entry.set_text("osc.udp://localhost:4243/")
        
        self.vbox.pack_start(gtk.Label("OSC Path"))
        self.oscpath_entry= gtk.Entry()
        self.vbox.pack_start(self.oscpath_entry)
        self.oscpath_entry.set_text("/moodpd/lamps/00/rgb")
        
        self.window.show_all()

        # show the window
        self.window.show()
        
        # we're running
        self.running= True

    def main(self):
        while self.running:
            gtk.main_iteration(True)
            
    def color_changed(self, colorselection, user_param1):
        col= colorselection.get_current_color()
        r= int(col.red/256)
        g= int(col.green/256)
        b= int(col.blue/256)
        
        msg= liblo.Message(self.oscpath_entry.get_text())
        msg.add(r)
        msg.add(g)
        msg.add(b)
        liblo.send(self.oscurl_entry.get_text(), msg)


if __name__ == "__main__":
    g= gui()
    g.main()


