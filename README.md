
## C++ Console Chatting Program
This project was developed with Visual Studio 2019 on Windows
It is built in Visual Studio 2019.

Server is the console chat server
BClient is a bot client
Client is a console client

### Server build
Get the code from https://github.com/Microsoft/hiredis
hiredis, open the win32_interop project
Configuration format: static library (.lib)
Windows SDK Version: 10.0.XX
Platform Toolset: Build with Visual Studio 2019 (v142)

Open the Server project by clicking Server.sln
path hiredis to include directory
Set the path to the library directory hiredis\lib\x64
Configuration format: application (.exe)
Windows SDK Version: 10.0.XX
Platform Toolset: Align with Visual Studio 2019 (v142)
Server project build

### BClient, Client build
BClient.sln and Client.sln respectively
Configuration format: application (.exe)
Windows SDK Version: 10.0.XX
Platform Toolset: Align with Visual Studio 2019 (v142)
Build the project

In Visual Studio 2019 project configuration properties
Configuration: Release
Platform: x64
Warning level: level 3 (/W3)
SDL Check: Yes (/sdl)