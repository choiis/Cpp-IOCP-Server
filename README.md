
## C++ Console Chatting Program
* This project was developed with Visual Studio 2019 on Windows
* It is built in Visual Studio 2019.

* Server is the console chat server
* BClient is a bot client
* Client is a console client

### Prepare project environment
* First, prepare the SQL Server DB
* To start the server, you need to write SQL Server information in /Server/Server/conf/SqlConnection.properties.
* There is table information in /Chat/chatTable.sql. Create it in SQL Server DB in advance

### How to use Jenkins to build this project
* You can make Jenkins pipeline or build job by referring to the contents written in Jenkinsfile.
* You need to download git and msbuild Jenkins plugins on Windows operating system.
* msbuild.exe links to C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\msbuild.exe

### Server build on Visual Studio 2019
* Open the Server project by clicking Server.sln
* In Visual Studio 2019 project configuration properties

```
Configuration format: application (.exe)

Windows SDK Version: 10.0.XX

Platform Toolset: Align with Visual Studio 2019 (v142)

Configuration: Release

Platform: x64

Warning level: level 3 (/W3)

SDL Check: No (/sdl)
```

### BClient, Client build on Visual Studio 2019
* Open BClient.sln and Client.sln respectively
* In Visual Studio 2019 project configuration properties

```
Configuration format: application (.exe)

Windows SDK Version: 10.0.XX

Platform Toolset: Align with Visual Studio 2019 (v142)

Configuration: Release

Platform: x64

Warning level: level 3 (/W3)

SDL Check: No (/sdl)
```
