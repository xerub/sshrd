/*
 *  ramdisk ssh server
 *
 *  Copyright (c) 2015 xerub
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOTypes.h>
#include "IOUSBDeviceControllerLib.h"

IOUSBDeviceDescriptionRef IOUSBDeviceDescriptionCreateWithType(CFAllocatorRef, CFStringRef);

io_service_t
get_service(const char *name, unsigned int retry)
{
    io_service_t service;
    CFDictionaryRef match = IOServiceMatching(name);

    while (1) {
        CFRetain(match);
        service = IOServiceGetMatchingService(kIOMasterPortDefault, match);
        if (service || !retry--) {
            break;
        }
        printf("Didn't find %s, trying again\n", name);
        sleep(1);
    }

    CFRelease(match);
    return service;
}

/* reversed from restored_external */
int
init_usb(void)
{
    int i;
    CFNumberRef n;
    io_service_t service;
    CFMutableDictionaryRef dict;
    IOUSBDeviceDescriptionRef desc;
    IOUSBDeviceControllerRef controller;

    desc = IOUSBDeviceDescriptionCreateWithType(kCFAllocatorDefault, CFSTR("standardMuxOnly")); /* standardRestore */
    if (!desc) {
        return -1;
    }
    IOUSBDeviceDescriptionSetSerialString(desc, CFSTR("ramdisk tool " __DATE__ " " __TIME__ ));

    controller = 0;
    while (IOUSBDeviceControllerCreate(kCFAllocatorDefault, &controller)) {
        printf("Unable to get USB device controller\n");
        sleep(3);
    }
    IOUSBDeviceControllerSetDescription(controller, desc);

    CFRelease(desc);
    CFRelease(controller);

    service = get_service("AppleUSBDeviceMux", 10);
    if (!service) {
        return -1;
    }

    dict = CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    i = 7;
    n = CFNumberCreate(NULL, kCFNumberIntType, &i);
    CFDictionarySetValue(dict, CFSTR("DebugLevel"), n);
    CFRelease(n);

    i = IORegistryEntrySetCFProperties(service, dict);
    CFRelease(dict);
    IOObjectRelease(service);

    return i;
}

#include "micro_inetd.c" /* I know, I am a bad person for doing this */
char *execve_params[] = { "micro_inetd", "22", "/usr/local/sbin/dropbear", "-i", NULL };

/* chopped from https://code.google.com/p/iphone-dataprotection/ */
int
main(int argc, char *argv[])
{
    printf("Starting ramdisk tool\n");
    printf("Compiled " __DATE__ " " __TIME__ "\n");

    io_service_t service = get_service("IOWatchDogTimer", 0);
    if (service) {
        int zero = 0;
        CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &zero);
        IORegistryEntrySetCFProperties(service, n);
        CFRelease(n);
        IOObjectRelease(service);
    }

    if (init_usb()) {
        printf("USB init FAIL\n");
    } else {
        printf("USB init done\n");
    }

    printf(" #######  ##    ##\n");
    printf("##     ## ##   ## \n");
    printf("##     ## ##  ##  \n");
    printf("##     ## #####   \n");
    printf("##     ## ##  ##  \n");
    printf("##     ## ##   ## \n"); 
    printf(" #######  ##    ##\n");

    printf("Running server\n");
    return main2(4, execve_params);
}
