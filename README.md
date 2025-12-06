# Cpp-IOCP-Server

High-performance Windows C++ Chat Server using **I/O Completion Ports
(IOCP)**, fully compatible with **Visual Studio 2026** and SQL Server.

This project includes:

-   ✔️ Console-based **Chat Server**
-   ✔️ Console **Client**
-   ✔️ Automated **Bot Client (BClient)**
-   ✔️ SQL Server integration
-   ✔️ Jenkins CI build pipeline
-   ✔️ Tested and runnable on **Visual Studio 2026 + Windows 11/10**

------------------------------------------------------------------------

## Overview

`Cpp-IOCP-Server` is a multithreaded TCP chat server implemented using
**Windows I/O Completion Ports (IOCP)** for scalable asynchronous
networking.

It is designed to handle many concurrent clients efficiently with
minimal CPU overhead.

### Key Components

  Component                    Description
  ---------------------------- -----------------------------------------------
  **Server**                   Console-based asynchronous IOCP TCP server
  **Client**                   Simple console client for manual chat testing
  **BClient**                  Bot client for load testing & automation
  **SQL Server Integration**   Stores users & chat logs
  **Jenkinsfile**              Windows-based MSBuild pipeline

------------------------------------------------------------------------

## Features

### High Performance Networking (IOCP)

-   Asynchronous TCP sockets\
-   Efficient completion port threading model\
-   Highly scalable for many concurrent users

### SQL Server Support

Connection settings stored in:

    Server/conf/SqlConnection.properties

Database schema:

    Chat/chatTable.sql

### Automated Bot Client

Useful for: - Load testing - Repetitive workflow tests - Simulating
large user groups

### CI/CD with Jenkins

-   Windows-based build pipeline using `Jenkinsfile`
-   Compatible with MSBuild 2026 toolset

------------------------------------------------------------------------

## Repository Structure

    /Server                     — Visual Studio 2026 chat server project
    /Client                     — Console client
    /BClient                    — Bot client
    /Chat                       — Shared protocol definitions
    /Chat/chatTable.sql         — SQL Server schema
    README.md                   — Documentation
    Jenkinsfile                 — Jenkins CI pipeline

------------------------------------------------------------------------

## Requirements

### Software

-   **Windows 10 / Windows 11**
-   **Visual Studio 2026**
    -   MSVC v146 Toolset\
    -   Windows SDK 10.0+
-   **SQL Server 2017+**
-   **Git (optional, for Jenkins builds)**

### Development Environment

Ensure the following VS2026 workloads are installed:

-   ✔ Desktop development with C++\
-   ✔ Windows 10/11 SDK\
-   ✔ MSVC v146 compiler

------------------------------------------------------------------------

## Build & Run Instructions

### 1️. Prepare SQL Server

1.  Create a database.
2.  Run:

```{=html}
<!-- -->
```
    Chat/chatTable.sql

3.  Configure DB connection:

```{=html}
<!-- -->
```
    Server/conf/SqlConnection.properties

Example:

    DB_HOST=127.0.0.1
    DB_USER=sa
    DB_PASSWORD=your_password
    DB_NAME=chatdb

------------------------------------------------------------------------

### 2️. Build the Server / Client / Bot

Open each `.sln` in **Visual Studio 2026**:

-   `Server/Server.sln`
-   `Client/Client.sln`
-   `BClient/BClient.sln`

Build:

    Configuration: Release
    Platform: x64

------------------------------------------------------------------------

### 3️. Run the Server

    Server.exe

You should see:

    Server ready listen
    port number : xxxx
    Server CPU num : xx

------------------------------------------------------------------------

### 4️. Run Clients

Start one or more:

    Client.exe
    BClient.exe

They will connect to the server and begin communication.

------------------------------------------------------------------------

## Bot Client Usage

The Bot Client (`BClient`) is capable of:

-   Auto-login\
-   Auto-message sending\
-   Load testing with many parallel bots

This allows verifying server stability under heavy traffic.

------------------------------------------------------------------------

##Jenkins CI Integration

Included `Jenkinsfile` supports automated builds on Windows:

-   Git checkout\
-   MSBuild compile\
-   Artifact generation

Configure Jenkins agent with:

-   Visual Studio 2026 Build Tools\
-   Git\
-   Windows Server 2019/2022 or Windows 11

------------------------------------------------------------------------

## Why IOCP?

IOCP allows:

-   High concurrency with fewer threads\
-   Low CPU usage\
-   Efficient overlapped I/O\
-   Ideal for chat servers, game servers, and streaming servers

