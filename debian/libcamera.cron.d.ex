#
# Regular cron jobs for the libcamera package
#
0 4	* * *	root	[ -x /usr/bin/libcamera_maintenance ] && /usr/bin/libcamera_maintenance
