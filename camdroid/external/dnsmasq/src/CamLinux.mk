LOCAL_PATH := $(call my-dir)

#########################

include $(CLEAR_VARS)
LOCAL_SRC_FILES :=  bpf.c cache.c dbus.c dhcp.c dnsmasq.c forward.c helper.c lease.c log.c \
                    netlink.c network.c option.c rfc1035.c rfc2131.c tftp.c util.c

LOCAL_MODULE := dnsmasq

LOCAL_C_INCLUDES := external/dnsmasq/src

LOCAL_CFLAGS := -O2 -g -W -Wall -DNO_IPV6 -DNO_TFTP -DNO_SCRIPT
LOCAL_SYSTEM_SHARED_LIBRARIES := libc libcutils

include $(BUILD_EXECUTABLE)

