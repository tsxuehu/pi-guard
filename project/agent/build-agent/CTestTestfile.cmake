# CMake generated Testfile for 
# Source directory: /home/tsxuehu/workspace-daoqi/pi-guard/project/agent
# Build directory: /home/tsxuehu/workspace-daoqi/pi-guard/project/agent/build-agent
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
subdirs("src/foundation")
subdirs("src/modules/capture/audio")
subdirs("src/modules/capture/video")
subdirs("src/modules/processing/echo_canceller")
subdirs("src/modules/processing/encoder")
subdirs("src/modules/processing/motion_detect")
subdirs("src/modules/output")
subdirs("src/modules/infra/config")
subdirs("src/modules/infra/event_bus")
subdirs("src/modules/infra/log")
subdirs("src/modules/infra/perf_monitor")
subdirs("src/modules/external_access/http")
subdirs("src/modules/external_access/pusher")
subdirs("src/modules/external_access/websocket")
subdirs("src/runtime")
subdirs("tests")
