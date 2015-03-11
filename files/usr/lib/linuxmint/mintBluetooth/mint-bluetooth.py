#!/usr/bin/env python2

import sys
import os
import gettext
from gi.repository import Gtk, GnomeBluetooth
import pwd
import socket

BLUETOOTH_DISABLED_PAGE      = "disabled-page"
BLUETOOTH_HW_DISABLED_PAGE   = "hw-disabled-page"
BLUETOOTH_NO_DEVICES_PAGE    = "no-devices-page"
BLUETOOTH_WORKING_PAGE       = "working-page"

# i18n
gettext.install("cinnamon", "/usr/share/locale")

class BluetoothConfig:
    ''' Create the UI '''
    def __init__(self):
        self.window = Gtk.Window(Gtk.WindowType.TOPLEVEL)

        self.window.set_title(_("Bluetooth"))
        self.window.set_icon_name("bluetooth")
        self.window.connect("destroy", Gtk.main_quit)

        self.main_box = Gtk.VBox()
        self.stack = Gtk.Stack()
        self.rf_switch = Gtk.Switch()

        self.add_stack_page(_("Bluetooth is disabled"), BLUETOOTH_DISABLED_PAGE);
        self.add_stack_page(_("No Bluetooth adapters found"), BLUETOOTH_NO_DEVICES_PAGE);
        self.add_stack_page(_("Bluetooth is disabled by hardware switch"), BLUETOOTH_HW_DISABLED_PAGE);

        self.lib_widget = GnomeBluetooth.SettingsWidget.new();

        self.stack.add_named(self.lib_widget, BLUETOOTH_WORKING_PAGE)

        self.lib_widget.show()
        self.stack.show();
        self.main_box.show();

        self.main_box.pack_start(self.stack, True, True, 0)
        self.window.add(self.main_box)

        self.window.show()

    def add_stack_page(self, message, name):
        label = Gtk.Label(message)
        self.stack.add_named(label, name)
        label.show()

if __name__ == "__main__":
    BluetoothConfig()
    Gtk.main()
