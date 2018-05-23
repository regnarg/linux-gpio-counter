# Linux GPIO pulse counter

A simple kernel module that counts pulses on a GPIO input.
If there are many pulses per second and you do not need to handle
each of them separately, this can reduce overhead compared to
handling each pulse in userspace (which may even lose some pulses
if handling is too slow).

Counters are exported in `/sys/class/gpio_counter`.
