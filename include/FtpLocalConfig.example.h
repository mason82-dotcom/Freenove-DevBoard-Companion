#pragma once
// Copy to include/FtpLocalConfig.h (git-ignored) to enable the local FTP server.
// FTP is unencrypted: user, password and files cross the home LAN in clear
// text. The server is OFF after boot, is switched on from the System page,
// accepts only clients from the device's own subnet, only passive mode, one
// client at a time, and switches itself off after 10 minutes without activity.
#define FTP_USER "YOUR_FTP_USER"
#define FTP_PASSWORD "YOUR_FTP_PASSWORD_MIN_8_CHARS"
#define FTP_PORT 21
#define FTP_PASV_PORT 50000
