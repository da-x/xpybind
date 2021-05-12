# xpybind

Note: this is code written back around 2003 with only slight mainainingship.

An old version of it is still hosted on sourceforge.net.


1. Preface

Since the invention of GUI, the concept of 'hotkeys' was widely accepted as a
method to execute functions within programs and environments. The common
definition of hotkey is comprised of combination of key presses, that include
'function' keys named Ctrl, Alt, Meta, Super, along with 'regular' keys such as
N, R, or F4.

Desktop environments today, such as KDE, allow the user to define hotkeys for
various intrinsic operations of the environment, such as resizing or closing
windows, and sometimes they allow the execution of external programs. However,
a hotkey by itself is sometimes not sufficient. The amount of combinations of a
single hotkey in the keyboard is limited. For example, say you have bound
Ctrl-L to the operation Location, and then you decide you'd also like to bind
the Lower operation. However, Ctrl-L, Alt-L, and Ctrl-Alt-L, etc., are already
bound, what would you do?

The developers of Emacs defined the concept of 'key sequence' that extends the
concept of a hotkey. A key sequence is a list of hotkeys. A function is
executed only after that particular list of hotkeys have been serially pressed.
Using this concept, you can order functions under an effective heirarchy. For
example, find-function-other-frame is bound to 'C-x 5 l', which means:
Control-X, then 5, then l, will execute that function. Other functions that act
on frames under emacs are defined under the sub-sequence 'C-x 5', so it is
rather easy to remember how to access them.

Under conventional desktop environments such as KDE, the concept of key
sequences doesn't exist in its pure form, probably because it is hard to
configure in GUI. However, advanced users, especially programmers and people
who have worked in emacs, would appreciate the ability to bind key sequences
globally in X. One application, keylaunch, currently provides that
functionality in X. However, it is limited to merely  running shell scripts
from those sequences. 'ratpoison', the window manager, started to support key
sequences after I contributed a patch for it. Ratpoison's key sequnces are
bound to ratpoison commands -- then again, not flexibly, and forces you to use
ratpoison.

XEmacs binds key sequences to its own elisp interpreter space -- of course, you
can only access these sequences from XEmacs and not globally from X.  But
unlike the other options, in XEmacs you can bind key sequences to complex
functions written in elisp, and they all run in the same interpreter space,
which means that you can have global variables that can be accessed from the
various functions.

So what we need is a small utility that provides these:

 * Globally bind key sequences in X to functions.
 * The functions need to run in one scriptable interpreter space.
 * Of course, instead of elisp, we can use a much more readble and
   widely accepted scripting language, such as Python.

That's xpybind.


2. Building and installing

In order to build xpybind, your systems need to contains the development
libraries and include files of XFree86 and Pytohn 2.3 or above. You also need
to have GNU/make and gcc.

To build xpybind simple run:

    make

To install, run:
 
    make install

xpybind is also distributed as a Debian package that can be retrieved
from the xpybind site.

Any user who would like to use xpybind will have to run it after X comes up. If
your desktop environment doesn't support running applications on startup, You
can manually set xpybind to run on X's startup by adding 'xpybind &' to your
~/.xinitrc script.

3. User configuration, for starters

If you don't know Python, I suggest that you surf to http://www.python.org
and start reading.

Done?

Good. After installing xpybind unto your system, create a new Python script as
~/.xpybind.

xpybind's configuration is quite simple. Your ~/.xpybind script needs to
contain one list of tuples, where each tuple is a key sequence definition
that contains the key sequence and the function to execute. The format
of the tuple is (key_sequence_string, callable_object).

The most simple configuration can be as follows:

    def my_func():
        print "Hello"

    bindings = KeySwitch(
        C_t=KeySwitch(
            f=my_func,
        )
    )

This will cause my_func to be executed if the key sequence 'Control-t f' is
performed.

But we probably want something more usable:

    def run_firefox():
        import os
        os.system("firefox &")

    def run_xchat():
        import os
        os.system("xchat &")

    bindings = KeySwitch(
        C_t = KeyChain('exec', KeySwitch(
             m=run_firefox,
             x=run_chat,
        )
    )

This will cause the sequence 'Control-t e x e c m' to execute firefox.

Defining a function for each operation can be considered code duplication.
Instead. define a function for each *type* of operation. Consider this example:

    def run_command(command_name):
        def runner():
            import os
            os.system("%s &" % (command_name, ))
        return runner

    bindings = KeySwitch(
        C_t = KeyChain('exec', KeySwitch(
             m=system("firefox &"),
             x=system("xchat &"),
        )
    )

Because it's Python we can access every Python package we would like from
our interpreter space, for example:

    import xmms
    bindings = KeySwitch(
        ...
        s=xmms.stop,
        ...
    )

That key sequence will effectively stop XMMS from playing when executed.

We can define more types of operations:

    def killall(name):
        def wrapper():
            try:
                os.system("killall %s" % (name, ))
            except:
                from traceback import print_exc
                print_exc()
                notify_to_use("Error killing %s" % (name, ))
        return wrapper

For example, if would like our commmand to be spawned in a terminal:

    def terminal(command):
        def wrapper():
            os.system("rxvt -e %s &" % (command, ))
        return wrapper

See the examples directory for more complete and useful configurations.


4. Functions and API of the xpybind module

After installing xpybind, run:

    python -c "import xpybind; help(xpybind)"
