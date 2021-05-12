# XPyBind
#
# Copyright (C) 2004 Dan Aloni <alonid@gmail.com>
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# ratpoison is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
# Boston, MA 02111-1307 USA

import os

# Exception handling

class Error(Exception): pass
class FocusLostError(Error) : pass

MODSEP = "_"

# KeySymMod is a tuple which contains a KeySym and a canoical representation
# of the modifiers. 

def _keysymmod_to_string(keysym, modifiers):
    """
    Convert between a keysym tuple (for example, (110, 4)), to a string
    representation of that tuple (C-t, for example).
    """
    mods = []
    for val, mod in xpb_modifiers:
        if modifiers & val:
            mods.append(mod)

    return MODSEP.join([[],[MODSEP.join(mods)]][len(mods) != 0] + [xpybind_c.keysym_to_string(keysym)])

def _string_to_keysymmod(s):
    """
    Convert between a string representation of a key combination (C-t, for example),
    to a keysym tuple (for example, (110, 4)).
    """
    parts = s.split(MODSEP)
    mod_parts = parts[:-1]
    key = parts[len(parts)-1]
    mods = []
    modifiers = 0

    for val, mod in xpb_modifiers:
        if mod in mod_parts:
            modifiers |= val
	    
    return (xpybind_c.string_to_keysym(key), modifiers)

def _grab_key(s):
    """
    Tells X to send as an event when the user press the specified key combination,
    which is given as a string ("C-t", for example). event_key will be called with
    the key combination when it is pressed.
    """
    keysymmod = _string_to_keysymmod(s)
    xpybind_c.grab_key(*keysymmod)

def _ungrab_key(s):
    """
    Tells X to stop from sending as an event when the user press the specified
    key combination, which is given as a string ("C-t", for example).
    """
    
    keysymmod = _string_to_keysymmod(s)
    xpybind_c.ungrab_key(*keysymmod)

def _read_key():
    """
    Blocks until the user hits a key combination. The key combination is returned
    as a string ('C-t', for example). If the focus from xpybind is lost while the
    function waits, FocusLostError is raised.
    """
    
    keysymmod = xpybind_c.read_key()
    if keysymmod == "focus lost":
        raise FocusLostError()
    return _keysymmod_to_string(*keysymmod)

def exit():
    """
    Tells the main loop of xpybind to exit.
    """
    xpybind_c.exit()

def notify_user(text, timeout=0):
    """
    This function displays a non-intrusive notification for the user. It is
    used by xpybind to notify the user about the currently expected hotkey
    when key sequences are entered. The Window displays itself without taking
    the keyboard focus from the user.

    The user can also use this function inside ~/.xpybind for notification
    about the success of operations that were carried out by key sequences
    or other activities.

    @timeout - The number of seconds until the window disappears from the
    moment it pops.
    """
    xpybind_c.uodate_input_window(text, timeout)

class KeyMatcherProcess(object):
    def __init__(self, keysym, modifiers):
        self.key_list = []
        self.keysym = keysym
        self.modifiers = modifiers
        self.dont_destory_on_exit = False

    def notify_user(self, text, timeout=0):
        notify_user(text, timeout=timeout)

    def unmatched_key(self, key):
        self.notify_user(' unmatched %s' % (key, ), 500)
        self.dont_destory_on_exit = True

    def run(self):
        s = _keysymmod_to_string(self.keysym, self.modifiers)
        last_window = xpybind_c.focus_input_window()
        xpybind_c.create_input_window()
        bindings = user_config.bindings
        
        try:
            try:
                bindings.match(s, self)
            except FocusLostError:
                print "Focus lost"
                xpybind_c.unfocus_input_window(0)
        finally:
            if not self.dont_destory_on_exit:
                xpybind_c.destroy_input_window()

    def continue_matcher(self, subobj):
        if callable(subobj):
            subobj()
        elif isinstance(subobj, KeyContinue):
            subobj.query(self)
   

class KeyContinue(object):
    def __init__(self):
        pass

    def next_keys(self):
        return []

    def title_prefix(self, kmp):
        return ' '.join(kmp.key_list)
    
    def title(self, kmp):
        return ' %s {%s}' % (self.title_prefix(kmp), self.next_title())

    def next_title(self):
        return ', '.join(self.next_keys())

    def match(self, key, kmp):
        pass

    def query(self, kmp):
        print "Expecting from list: %s" % ', '.join(self.next_keys())
        kmp.notify_user(self.title(kmp))
        self.match(_read_key(), kmp)

class KeySwitchVirt(KeyContinue):
    def next_keys(self):
        return self.get_subs().keys()

    def match(self, key, kmp):
        subobj = self.get_subs().get(key, None)
        if not subobj:
            kmp.unmatched_key(key)
            return

        kmp.key_list.append(key)
        kmp.continue_matcher(subobj)

def KeyChain(chain, obj):
    chain_rev = list(chain)
    chain_rev.reverse()
    for c in chain_rev:
        kw = {}
        kw[c] = obj
        obj = KeySwitch(**kw)
    return obj

class KeySwitch(KeySwitchVirt):
    def __init__(self, **kw):
        self._subkeys = kw

    def get_subs(self):
        return self._subkeys

class KeyOnInvoke(KeySwitchVirt):
    def __init__(self, **kw):
        self._subkeys = None

    def subs_on_invoke(self):
        return {}

    def get_subs(self):
        if not self._subkeys:
            self._subkeys = self.subs_on_invoke()
        return self._subkeys

class KeyCompletion(KeyContinue):
    order_cache = []
    
    def __init__(self, receiver):
        self.receiver = receiver

    def get_strings(self):
        raise NotImplemented()

    def query(self, kmp):
        strings = list(self.get_strings())
        removed = []
        for choice in self.order_cache:
            if choice in strings:
                strings.remove(choice)
                removed.append(choice)
        strings = removed + strings
        options = []
        composed_string = ''
        filtered_strings = strings
        
        while len(filtered_strings) > 1:                
            kmp.notify_user(' ' + self.title_prefix(kmp) + ' %s {%s}' % (
                composed_string, (', '.join(filtered_strings)), ))
            
            key = _read_key()
            if key == 'period':
                key = '.'
                
            if key == 'BackSpace':
                if options:
                    composed_string = composed_string[:-1]
                    filtered_strings = options[-1]
                    del options[-1]
                continue
            
            if key == 'Return':
                filtered_strings = [filtered_strings[0]]
                break

            if key == 'Left':
                filtered_strings = [filtered_strings[-1]] + filtered_strings[:-1]
                continue
            
            if key == 'Right':
                filtered_strings = filtered_strings[1:] + [filtered_strings[0]]
                continue

            if len(key) != 1:
                kmp.unmatched_key(key)
                break

            composed_string += key
            options.append(filtered_strings)

            if not composed_string:
                filtered_strings = strings
            else:
                filtered_strings = []
                for s in strings:
                    if composed_string in s:
                        filtered_strings.append(s)            
            
        if len(filtered_strings) != 1:
            return

        choice = filtered_strings[0]
        if choice in self.order_cache:
            self.order_cache.remove(choice)
        self.order_cache.insert(0, choice)
        if len(self.order_cache) > 1000:
            del self.order_cache[1000]
        kmp.continue_matcher(self.receiver(choice))
         
def _event_key(keysym, modifiers):
    """
    This function is called by xpybind.c when one of the keys that were bound
    by grab_key() was hit. The two parameters that are passed to this function
    aer the KeySym and the canonical representation of the modifiers.

    By examining the keyboard_maps tree, the function decides whether to:
    1. Wait for more keys by using read_key()
    2. Call a function that is bound to the key combination sequence according
       to the tree.
    3. Do nothing in the case where nothing is bound to the key combination
       sequence it intercepted.
    """

    km = KeyMatcherProcess(keysym, modifiers)
    km.run()
    
def _map_keys():
    keys = user_config.bindings.next_keys()
    for key in keys:
        _grab_key(key)
    print "Grabbing %s" % ' '.join(keys)

def _unmap_keys():
    """
    Unmap the keys that were mapped by map_keys().
    """

    keys = user_config.bindings.next_keys()
    
    print "Ungrabbing %s" % ' '.join(keys)
    for key in keys:
        _ungrab_key(key)
        
def _get_user_config():
    import imp
    user_config = imp.new_module('xpybind_user_config')
    print "Loading settings from %s" % (user_settings_filename, )
    execfile(user_settings_filename, vars(user_config), vars(user_config))
    return user_config

def _load_user_config():
    global user_config
    try:
        config = _get_user_config()
    except:
	from traceback import print_exc
        print "Error loading user settings from %s" % (user_settings_filename, )
	print_exc()
        return
    user_config = config

def reload_user_config():
    """
    Reload the user's configuration. It is useful to bind this function
    to a key combination sequence in the user settings so the user would
    be able to reload her settings.
    """

    _unmap_keys()
    _load_user_config()
    _map_keys()
    
def _default_user_config():
    global user_config
    import imp

    user_config = imp.new_module('xpybind_user_config')    
    user_config.bindings = KeySwitch(
        Menu = KeySwitch(
            r=reload_user_config,
            Return=(lambda:None),
            Escape=exit
        )
    )  

def _init(settings_filename=None):
    """
    Called by xpybind.c to initialize the module.
    """

    global xpybind_c
    import xpybind_c
    
    global xpb_modifiers
    xpb_modifiers = zip(*xpybind_c.get_xpb_modifiers())

    print "xpybind python extension initialized"

    global user_settings_filename
    if settings_filename:
        user_settings_filename = settings_filename
    else:
        user_settings_filename = "%s/.xpybind" % os.getenv("HOME")

    _default_user_config()
    _load_user_config()
    _map_keys()
