# Security

This is a kernel driver, so a bug in it can crash the system or expose memory. To report a security problem, use GitHub's private vulnerability reporting on this repository: open the **Security** tab and choose **Report a vulnerability**. That keeps the report private until a fix is ready.

Don't open a public issue for a security problem.

Include the driver version (`modinfo -F version HwsUHDX1Capture`), the kernel version (`uname -r`), and the steps that show the problem.

Only the newest release gets fixes.
