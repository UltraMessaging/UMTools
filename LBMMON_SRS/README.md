# LBMMON_SRS

When UM version 6.15 was released, the SRS component was enhanced to
send its daemon stats via protocol buffers,
which makes the data availalbe to the general UM statistics
infrastructure, including the "lbmmon" library.

The "MCS" component was enhanced to read and process SRS daemon stats.

However, the SRS additions to the "lbmmon.c" and "lbmmon.java" example apps
were not ready in time for the 6.15 release.
So those files are being made available here, on GitHub.

Informatica expects the 6.16 release to include these additions
in the main packages.
