#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nvml.h>

#define MAX_DEVICES 16
#define MAX_NAME_LEN 256

typedef struct {
    int device_id;
    int show_info;
    int show_fan;
    int show_power;
    int set_power_limit;
    unsigned int power_limit;
} cli_args_t;

static void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -d, --device ID     Select GPU device by index (default: 0)\n");
    printf("  -i, --info          Show device information\n");
    printf("  -f, --fan           Show fan information\n");
    printf("  -p, --power         Show power information\n");
    printf("  --set-power WATTS   Set power limit in watts\n");
    printf("  -h, --help          Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s -i              # Show info for GPU 0\n", program_name);
    printf("  %s -d 1 -f         # Show fan info for GPU 1\n", program_name);
    printf("  %s -p              # Show power info for GPU 0\n", program_name);
    printf("  %s --set-power 250 # Set power limit to 250W for GPU 0\n", program_name);
}

static const char* nvml_error_string(nvmlReturn_t result) {
    switch (result) {
        case NVML_SUCCESS:
            return "Success";
        case NVML_ERROR_UNINITIALIZED:
            return "NVML not initialized";
        case NVML_ERROR_INVALID_ARGUMENT:
            return "Invalid argument";
        case NVML_ERROR_NOT_SUPPORTED:
            return "Not supported";
        case NVML_ERROR_NO_PERMISSION:
            return "Insufficient permissions";
        case NVML_ERROR_ALREADY_INITIALIZED:
            return "Already initialized";
        case NVML_ERROR_NOT_FOUND:
            return "Not found";
        case NVML_ERROR_INSUFFICIENT_SIZE:
            return "Insufficient size";
        case NVML_ERROR_INSUFFICIENT_POWER:
            return "Insufficient power";
        case NVML_ERROR_DRIVER_NOT_LOADED:
            return "Driver not loaded";
        case NVML_ERROR_TIMEOUT:
            return "Timeout";
        case NVML_ERROR_IRQ_ISSUE:
            return "IRQ issue";
        case NVML_ERROR_LIBRARY_NOT_FOUND:
            return "Library not found";
        case NVML_ERROR_FUNCTION_NOT_FOUND:
            return "Function not found";
        case NVML_ERROR_CORRUPTED_INFOROM:
            return "Corrupted InfoROM";
        case NVML_ERROR_GPU_IS_LOST:
            return "GPU is lost";
        case NVML_ERROR_RESET_REQUIRED:
            return "Reset required";
        case NVML_ERROR_OPERATING_SYSTEM:
            return "Operating system error";
        case NVML_ERROR_LIB_RM_VERSION_MISMATCH:
            return "Library/RM version mismatch";
        case NVML_ERROR_IN_USE:
            return "In use";
        case NVML_ERROR_MEMORY:
            return "Memory error";
        case NVML_ERROR_NO_DATA:
            return "No data";
        case NVML_ERROR_VGPU_ECC_NOT_SUPPORTED:
            return "vGPU ECC not supported";
        case NVML_ERROR_INSUFFICIENT_RESOURCES:
            return "Insufficient resources";
        default:
            return "Unknown error";
    }
}

static int show_device_info(nvmlDevice_t device, int device_id) {
    nvmlReturn_t result;
    char name[MAX_NAME_LEN];
    char uuid[MAX_NAME_LEN];
    unsigned int temperature;
    nvmlMemory_t memory;
    
    printf("=== Device %d Information ===\n", device_id);
    
    result = nvmlDeviceGetName(device, name, sizeof(name));
    if (result == NVML_SUCCESS) {
        printf("Name: %s\n", name);
    } else {
        printf("Name: Error getting name (%s)\n", nvml_error_string(result));
    }
    
    result = nvmlDeviceGetUUID(device, uuid, sizeof(uuid));
    if (result == NVML_SUCCESS) {
        printf("UUID: %s\n", uuid);
    } else {
        printf("UUID: Error getting UUID (%s)\n", nvml_error_string(result));
    }
    
    result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
    if (result == NVML_SUCCESS) {
        printf("Temperature: %u°C\n", temperature);
    } else {
        printf("Temperature: Error getting temperature (%s)\n", nvml_error_string(result));
    }
    
    result = nvmlDeviceGetMemoryInfo(device, &memory);
    if (result == NVML_SUCCESS) {
        printf("Memory Total: %llu MB\n", memory.total / (1024 * 1024));
        printf("Memory Used: %llu MB\n", memory.used / (1024 * 1024));
        printf("Memory Free: %llu MB\n", memory.free / (1024 * 1024));
    } else {
        printf("Memory: Error getting memory info (%s)\n", nvml_error_string(result));
    }
    
    return 0;
}

static int show_fan_info(nvmlDevice_t device, int device_id) {
    nvmlReturn_t result;
    unsigned int fan_speed;
    
    printf("=== Device %d Fan Information ===\n", device_id);
    
    result = nvmlDeviceGetFanSpeed(device, &fan_speed);
    if (result == NVML_SUCCESS) {
        printf("Fan Speed: %u%%\n", fan_speed);
    } else {
        printf("Fan Speed: Error getting fan speed (%s)\n", nvml_error_string(result));
        return 1;
    }
    
    return 0;
}

static int show_power_info(nvmlDevice_t device, int device_id) {
    nvmlReturn_t result;
    unsigned int power_usage;
    unsigned int power_limit;
    unsigned int min_limit, max_limit;
    unsigned int default_limit;
    
    printf("=== Device %d Power Information ===\n", device_id);
    
    result = nvmlDeviceGetPowerUsage(device, &power_usage);
    if (result == NVML_SUCCESS) {
        printf("Power Usage: %u mW (%.2f W)\n", power_usage, power_usage / 1000.0);
    } else {
        printf("Power Usage: Error getting power usage (%s)\n", nvml_error_string(result));
    }
    
    result = nvmlDeviceGetPowerManagementLimit(device, &power_limit);
    if (result == NVML_SUCCESS) {
        printf("Power Limit: %u mW (%.2f W)\n", power_limit, power_limit / 1000.0);
    } else {
        printf("Power Limit: Error getting power limit (%s)\n", nvml_error_string(result));
    }
    
    result = nvmlDeviceGetPowerManagementLimitConstraints(device, &min_limit, &max_limit);
    if (result == NVML_SUCCESS) {
        printf("Power Limit Range: %u - %u mW (%.2f - %.2f W)\n", 
               min_limit, max_limit, min_limit / 1000.0, max_limit / 1000.0);
    } else {
        printf("Power Limit Range: Error getting power limit constraints (%s)\n", nvml_error_string(result));
    }
    
    result = nvmlDeviceGetPowerManagementDefaultLimit(device, &default_limit);
    if (result == NVML_SUCCESS) {
        printf("Default Power Limit: %u mW (%.2f W)\n", default_limit, default_limit / 1000.0);
    } else {
        printf("Default Power Limit: Error getting default power limit (%s)\n", nvml_error_string(result));
    }
    
    return 0;
}

static int set_power_limit(nvmlDevice_t device, int device_id, unsigned int limit_watts) {
    nvmlReturn_t result;
    unsigned int limit_mw = limit_watts * 1000;
    unsigned int min_limit, max_limit;
    
    printf("=== Setting Power Limit for Device %d ===\n", device_id);
    
    result = nvmlDeviceGetPowerManagementLimitConstraints(device, &min_limit, &max_limit);
    if (result != NVML_SUCCESS) {
        printf("Error: Cannot get power limit constraints (%s)\n", nvml_error_string(result));
        return 1;
    }
    
    if (limit_mw < min_limit || limit_mw > max_limit) {
        printf("Error: Power limit %u W is outside valid range (%.2f - %.2f W)\n", 
               limit_watts, min_limit / 1000.0, max_limit / 1000.0);
        return 1;
    }
    
    result = nvmlDeviceSetPowerManagementLimit(device, limit_mw);
    if (result == NVML_SUCCESS) {
        printf("Successfully set power limit to %u W\n", limit_watts);
    } else {
        printf("Error: Failed to set power limit (%s)\n", nvml_error_string(result));
        return 1;
    }
    
    return 0;
}

static int parse_args(int argc, char* argv[], cli_args_t* args) {
    memset(args, 0, sizeof(cli_args_t));
    args->device_id = 0;
    
    if (argc == 1) {
        args->show_info = 1;
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            return -1;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
            if (i + 1 >= argc) {
                printf("Error: --device requires a device ID\n");
                return -1;
            }
            args->device_id = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--info") == 0) {
            args->show_info = 1;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fan") == 0) {
            args->show_fan = 1;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--power") == 0) {
            args->show_power = 1;
        } else if (strcmp(argv[i], "--set-power") == 0) {
            if (i + 1 >= argc) {
                printf("Error: --set-power requires a power limit in watts\n");
                return -1;
            }
            args->set_power_limit = 1;
            args->power_limit = atoi(argv[++i]);
        } else {
            printf("Error: Unknown option '%s'\n", argv[i]);
            return -1;
        }
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    nvmlReturn_t result;
    cli_args_t args;
    unsigned int device_count;
    nvmlDevice_t device;
    
    if (parse_args(argc, argv, &args) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    
    result = nvmlInit();
    if (result != NVML_SUCCESS) {
        printf("Error: Failed to initialize NVML (%s)\n", nvml_error_string(result));
        return 1;
    }
    
    result = nvmlDeviceGetCount(&device_count);
    if (result != NVML_SUCCESS) {
        printf("Error: Failed to get device count (%s)\n", nvml_error_string(result));
        nvmlShutdown();
        return 1;
    }
    
    if (device_count == 0) {
        printf("No NVIDIA GPUs found\n");
        nvmlShutdown();
        return 1;
    }
    
    if (args.device_id >= (int)device_count) {
        printf("Error: Device ID %d not found (available: 0-%d)\n", args.device_id, device_count - 1);
        nvmlShutdown();
        return 1;
    }
    
    result = nvmlDeviceGetHandleByIndex(args.device_id, &device);
    if (result != NVML_SUCCESS) {
        printf("Error: Failed to get device handle for device %d (%s)\n", args.device_id, nvml_error_string(result));
        nvmlShutdown();
        return 1;
    }
    
    if (args.show_info) {
        show_device_info(device, args.device_id);
    }
    
    if (args.show_fan) {
        show_fan_info(device, args.device_id);
    }
    
    if (args.show_power) {
        show_power_info(device, args.device_id);
    }
    
    if (args.set_power_limit) {
        set_power_limit(device, args.device_id, args.power_limit);
    }
    
    nvmlShutdown();
    return 0;
}