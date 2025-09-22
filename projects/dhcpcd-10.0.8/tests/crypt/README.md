# dhcpcd Test Suite

Currently this just tests the RFC2202 MD5 implementation in dhcpcd.
This is important, because dhcpcd will either use the system MD5
implementation if found, otherwise some compat code.

This test suit ensures that it works in accordance with known standards
on your platform.
