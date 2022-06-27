# EXCDS Bridge

If you know what the project is for, the title is probably self explanatory.

This EuroScope plugin is used for connecting the EXCDS program (link coming soon) to EuroScope. This allows the program to execute
commands into EuroScope. You can click a button in EXCDS and see the change in EuroScope right away!

## What it is
The Bridge is written in C++ (language supported by EuroScope) using the EuroScope library. We are also using the
[Socket.IO C++ Client](https://github.com/socketio/socket.io-client-cpp) to communicate between the programs.

The socket.io protocol can be used in almost any programming language which makes it very accessable. This will allow other services
to communicate with the bridge if they wish.

For now, it uses `localhost` port `7500`. This will be configurable in the future.

## Frontend
Probably the most important part of this project is the actual program itself. Once a link is available, it will be put here.

Written by [Liam Shaw (LiamS16)](https://github.com/LiamS16).

## License
The bridge (to be clear, this is not including the actual EXCDS program) is covered under the
`Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)` license 
(details can be found at: https://creativecommons.org/licenses/by-sa/4.0/).

The full license is located in this repository and can be found [here](LICENSE).