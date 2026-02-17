ARS Enhanced.

# Features:
    1. Execute remote commands.
    2. Persistant across reboots.
    3. Debugging capabilities.
    4. Need technical knowledge to uninstall.

# Available Commmands:
    say
    restart

# Example:
    -> say "sky aint blue"
    -> say "hello world" 5
    -> say "restarting now"
    -> restart

All valid commands follow this pattern
    command
    command N
    command "message"
    command "message" N

command = say or restart
N = number (repeat count)
"message" = text in quotes


# Random Key Mode
- now ARS works based on random key on every 5 seconds
- Every 5 secs randomly a key is selected and upon pressing that key it reboots

