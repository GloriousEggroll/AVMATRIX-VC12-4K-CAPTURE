gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,format=YUY2,width=1920,height=1080,framerate=60/1 ! queue2 ! videorate ! videoscale ! videoconvert ! xvimagesink
