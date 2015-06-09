#ifndef RTATEST_CL_UTILITY_H
#define RTATEST_CL_UTILITY_H

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>
#include <iostream>
#include <string>
#include <fstream>

std::string loadProgram(std::string input)
{
    std::ifstream stream(input.c_str());
    if (!stream.is_open()) {
        std::cout << "Cannot open file: " << input << std::endl;
        exit(1);
    }
    return std::string(std::istreambuf_iterator<char>(stream),
            (std::istreambuf_iterator<char>()));
}

void printPlatforms(std::vector<cl::Platform>& platforms) {
    std::string info;
    for(unsigned int i=0; i<platforms.size(); i++) {
        cl::Platform platform = platforms[i];
        std::cout << "----------------------------" << std::endl;
        std::cout << "Platform " << i << std::endl;
        platform.getInfo(CL_PLATFORM_NAME, &info);
        std::cout << info << std::endl;
        platform.getInfo(CL_PLATFORM_VERSION, &info);
        std::cout << info << std::endl;
        platform.getInfo(CL_PLATFORM_VENDOR, &info);
        std::cout << info << std::endl;
    }
    std::cout << "----------------------------" << std::endl;
}

void printDevices(std::vector<cl::Device>& devices) {
    std::vector<::size_t> wgsizes;
    for(unsigned int i=0; i<devices.size(); i++) {
        cl::Device device = devices[i];
        std::cout << "----------------------------" << std::endl;
        std::cout << "Device " << i << " info:" << std::endl;
        std::string info;
        device.getInfo(CL_DEVICE_NAME, &info);
        std::cout << info << std::endl;
        device.getInfo(CL_DEVICE_VENDOR, &info);
        std::cout << info << std::endl;
        device.getInfo(CL_DEVICE_VERSION, &info);
        std::cout << info << std::endl;
        device.getInfo(CL_DRIVER_VERSION, &info);
        std::cout << info << std::endl;
        std::cout << "Work item sizes: ";
        device.getInfo(CL_DEVICE_MAX_WORK_ITEM_SIZES, &wgsizes);
        for(unsigned int j=0; j<wgsizes.size(); j++)
            std::cout << wgsizes[j] << " ";
        std::cout << std::endl;
        ::size_t wgsize;
        device.getInfo(CL_DEVICE_MAX_WORK_GROUP_SIZE, &wgsize);
        std::cout << "Max work group size: " << wgsize << std::endl;
        cl_ulong maxAlloc;
        device.getInfo(CL_DEVICE_MAX_MEM_ALLOC_SIZE, &maxAlloc);
        std::cout << "Max alloc: " << maxAlloc << std::endl;
    }
    std::cout << "----------------------------" << std::endl;
}

#endif
