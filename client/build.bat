@echo off
echo Building Client...
g++ -o client.exe client.cpp ../common/crypto_utils.cpp ../common/network_utils.cpp ../common/file_transfer.cpp ../common/session_manager.cpp -lws2_32 -lcrypt32 -std=c++17 -static
if %errorlevel% == 0 (
    echo Client built successfully!
) else (
    echo Build failed!
)
pause