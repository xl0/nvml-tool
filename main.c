#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include </usr/local/cuda-12.9/targets/x86_64-linux/include/nvml.h>
#include <ctype.h>

#define MAX_DEVICES 64
#define MAX_NAME_LEN 256
#define MAX_UUID_LEN 80

typedef enum {
    TEMP_CELSIUS,
    TEMP_FAHRENHEIT,
    TEMP_KELVIN
} temp_unit_t;

typedef enum {
    CMD_NONE,
    CMD_INFO,
    CMD_POWER,
    CMD_FAN,
    CMD_TEMP,
    CMD_STATUS,
    CMD_LIST
} command_t;

typedef enum {
    SUBCMD_NONE,
    SUBCMD_SET,
    SUBCMD_RESTORE,
    SUBCMD_JSON
} subcommand_t;

typedef struct {
    int devices[MAX_DEVICES];
    int device_count;
    int all_devices;
    char uuid[MAX_UUID_LEN];
    int use_uuid;
    command_t command;
    subcommand_t subcommand;
    unsigned int set_value;
    temp_unit_t temp_unit;
} cli_args_t;

static void print_usage(const char* program_name) {
    printf("Usage: %s <command> [subcommand] [options] [args]\n", program_name);
    printf("\nCommands:\n");
    printf("  info [json]         Show comprehensive device information\n");
    printf("  power [set VALUE]   Show/set power usage and limits\n");
    printf("  fan [set VALUE]     Show/set fan speed (NVML v12+)\n");
    printf("  fan restore         Restore automatic fan control\n");
    printf("  temp                Show temperature\n");
    printf("  status              Show compact status overview\n");
    printf("  list                List all GPUs with index, UUID, and name\n");
    printf("\nDevice Selection:\n");
    printf("  -d, --device LIST   Select devices (default: all)\n");
    printf("                      Examples: -d 0  -d 0-2  -d 0,2,4\n");
    printf("  -u, --uuid UUID     Select device by UUID\n");
    printf("\nOutput Options:\n");
    printf("  --temp-unit UNIT    Temperature unit: C, F, K (default: C)\n");
    printf("  -h, --help          Show this help\n");
    printf("\nExamples:\n");
    printf("  %s info                    # Show info for all devices\n", program_name);
    printf("  %s info -d 0              # Show info for device 0\n", program_name);
    printf("  %s power -d 0-2           # Show power for devices 0,1,2\n", program_name);
    printf("  %s power set 250 -d 1     # Set 250W limit on device 1\n", program_name);
    printf("  %s fan                    # Show fan speeds for all devices\n", program_name);
    printf("  %s fan set 80 -d 1        # Set 80%% fan speed on device 1\n", program_name);
    printf("  %s fan restore            # Restore automatic control\n", program_name);
    printf("  %s info json              # JSON info for all devices\n", program_name);
}


static double convert_temperature(unsigned int temp_c, temp_unit_t unit) {
    switch (unit) {
        case TEMP_CELSIUS: return temp_c;
        case TEMP_FAHRENHEIT: return (temp_c * 9.0 / 5.0) + 32.0;
        case TEMP_KELVIN: return temp_c + 273.15;
        default: return temp_c;
    }
}

static const char* temp_unit_string(temp_unit_t unit) {
    switch (unit) {
        case TEMP_CELSIUS: return "C";
        case TEMP_FAHRENHEIT: return "F";
        case TEMP_KELVIN: return "K";
        default: return "C";
    }
}

static int parse_device_range(const char* range_str, int* devices, int max_devices) {
    char* str = strdup(range_str);
    char* token = strtok(str, ",");
    int count = 0;
    
    while (token && count < max_devices) {
        char* dash = strchr(token, '-');
        if (dash) {
            *dash = '\0';
            int start = atoi(token);
            int end = atoi(dash + 1);
            for (int i = start; i <= end && count < max_devices; i++) {
                devices[count++] = i;
            }
        } else {
            devices[count++] = atoi(token);
        }
        token = strtok(NULL, ",");
    }
    
    free(str);
    return count;
}

static int find_device_by_uuid(const char* uuid, unsigned int device_count) {
    for (unsigned int i = 0; i < device_count; i++) {
        nvmlDevice_t device;
        char device_uuid[MAX_UUID_LEN];
        
        if (nvmlDeviceGetHandleByIndex(i, &device) == NVML_SUCCESS &&
            nvmlDeviceGetUUID(device, device_uuid, sizeof(device_uuid)) == NVML_SUCCESS) {
            if (strstr(device_uuid, uuid) != NULL) {
                return i;
            }
        }
    }
    return -1;
}

static void print_device_info_human(nvmlDevice_t device, int device_id, temp_unit_t temp_unit) {
    nvmlReturn_t result;
    char name[MAX_NAME_LEN];
    char uuid[MAX_UUID_LEN];
    unsigned int temperature;
    nvmlMemory_t memory;
    unsigned int fan_speed;
    unsigned int power_usage, power_limit;
    
    printf("=== Device %d", device_id);
    
    result = nvmlDeviceGetName(device, name, sizeof(name));
    if (result == NVML_SUCCESS) {
        printf(": %s", name);
    }
    printf(" ===\n");
    
    result = nvmlDeviceGetUUID(device, uuid, sizeof(uuid));
    if (result == NVML_SUCCESS) {
        printf("UUID: %s\n", uuid);
    }
    
    result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
    if (result == NVML_SUCCESS) {
        double temp = convert_temperature(temperature, temp_unit);
        printf("Temperature: %.1f%s\n", temp, temp_unit_string(temp_unit));
    }
    
    result = nvmlDeviceGetMemoryInfo(device, &memory);
    if (result == NVML_SUCCESS) {
        double used_pct = (double)memory.used / memory.total * 100.0;
        printf("Memory: %llu MB / %llu MB (%.1f%%)\n", 
               memory.used / (1024 * 1024), memory.total / (1024 * 1024), used_pct);
    }
    
    result = nvmlDeviceGetFanSpeed(device, &fan_speed);
    if (result == NVML_SUCCESS) {
        printf("Fan Speed: %u%%\n", fan_speed);
    }
    
    result = nvmlDeviceGetPowerUsage(device, &power_usage);
    if (result == NVML_SUCCESS) {
        nvmlDeviceGetPowerManagementLimit(device, &power_limit);
        double power_pct = (double)power_usage / power_limit * 100.0;
        printf("Power: %.2fW / %.2fW (%.1f%%)\n", 
               power_usage / 1000.0, power_limit / 1000.0, power_pct);
    }
    
    printf("\n");
}

static void print_device_info_json(nvmlDevice_t device, int device_id, temp_unit_t temp_unit, int is_last) {
    nvmlReturn_t result;
    char name[MAX_NAME_LEN] = "Unknown";
    char uuid[MAX_UUID_LEN] = "Unknown";
    unsigned int temperature = 0;
    nvmlMemory_t memory = {0};
    unsigned int fan_speed = 0;
    unsigned int power_usage = 0, power_limit = 0;
    
    nvmlDeviceGetName(device, name, sizeof(name));
    nvmlDeviceGetUUID(device, uuid, sizeof(uuid));
    nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
    nvmlDeviceGetMemoryInfo(device, &memory);
    nvmlDeviceGetFanSpeed(device, &fan_speed);
    nvmlDeviceGetPowerUsage(device, &power_usage);
    nvmlDeviceGetPowerManagementLimit(device, &power_limit);
    
    printf("  {\n");
    printf("    \"device_id\": %d,\n", device_id);
    printf("    \"name\": \"%s\",\n", name);
    printf("    \"uuid\": \"%s\",\n", uuid);
    printf("    \"temperature\": %.1f,\n", convert_temperature(temperature, temp_unit));
    printf("    \"temperature_unit\": \"%s\",\n", temp_unit_string(temp_unit));
    printf("    \"memory_total_mb\": %llu,\n", memory.total / (1024 * 1024));
    printf("    \"memory_used_mb\": %llu,\n", memory.used / (1024 * 1024));
    printf("    \"memory_free_mb\": %llu,\n", memory.free / (1024 * 1024));
    printf("    \"fan_speed_percent\": %u,\n", fan_speed);
    printf("    \"power_usage_watts\": %.2f,\n", power_usage / 1000.0);
    printf("    \"power_limit_watts\": %.2f\n", power_limit / 1000.0);
    printf("  }%s\n", is_last ? "" : ",");
}

static void print_power_cli(nvmlDevice_t device, int device_id, int single_device) {
    unsigned int power_usage;
    nvmlReturn_t result = nvmlDeviceGetPowerUsage(device, &power_usage);
    
    if (result == NVML_SUCCESS) {
        if (single_device) {
            printf("%.2f\n", power_usage / 1000.0);
        } else {
            printf("%d:%.2f\n", device_id, power_usage / 1000.0);
        }
    } else {
        if (single_device) {
            fprintf(stderr, "Error: %s\n", nvmlErrorString(result));
        } else {
            fprintf(stderr, "%d:Error: %s\n", device_id, nvmlErrorString(result));
        }
    }
}

static void print_fan_cli(nvmlDevice_t device, int device_id, int single_device) {
    unsigned int fan_speed;
    nvmlReturn_t result = nvmlDeviceGetFanSpeed(device, &fan_speed);
    
    if (result == NVML_SUCCESS) {
        printf("%d:%u\n", device_id, fan_speed);
    } else {
        if (single_device) {
            fprintf(stderr, "Error: %s\n", nvmlErrorString(result));
        } else {
            fprintf(stderr, "%d:Error: %s\n", device_id, nvmlErrorString(result));
        }
    }
}

static void print_temp_cli(nvmlDevice_t device, int device_id, temp_unit_t temp_unit, int single_device) {
    unsigned int temperature;
    nvmlReturn_t result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
    
    if (result == NVML_SUCCESS) {
        double temp = convert_temperature(temperature, temp_unit);
        if (single_device) {
            printf("%d:%.1f\n", device_id, temp);
        } else {
            printf("%d:%.1f\n", device_id, temp);
        }
    } else {
        if (single_device) {
            fprintf(stderr, "Error: %s\n", nvmlErrorString(result));
        } else {
            fprintf(stderr, "%d:Error: %s\n", device_id, nvmlErrorString(result));
        }
    }
}

static void print_status_cli(nvmlDevice_t device, int device_id, temp_unit_t temp_unit) {
    unsigned int temperature = 0, fan_speed = 0, power_usage = 0;
    
    nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
    nvmlDeviceGetFanSpeed(device, &fan_speed);
    nvmlDeviceGetPowerUsage(device, &power_usage);
    
    double temp = convert_temperature(temperature, temp_unit);
    printf("%d:%.1f%s,%u%%,%.1fW\n", device_id, temp, temp_unit_string(temp_unit), 
           fan_speed, power_usage / 1000.0);
}

static int parse_args(int argc, char* argv[], cli_args_t* args) {
    memset(args, 0, sizeof(cli_args_t));
    args->temp_unit = TEMP_CELSIUS;
    args->all_devices = 1;
    
    if (argc < 2) {
        return -1;
    }
    
    // Parse command
    if (strcmp(argv[1], "info") == 0) {
        args->command = CMD_INFO;
    } else if (strcmp(argv[1], "power") == 0) {
        args->command = CMD_POWER;
    } else if (strcmp(argv[1], "fan") == 0) {
        args->command = CMD_FAN;
    } else if (strcmp(argv[1], "temp") == 0) {
        args->command = CMD_TEMP;
    } else if (strcmp(argv[1], "status") == 0) {
        args->command = CMD_STATUS;
    } else if (strcmp(argv[1], "list") == 0) {
        args->command = CMD_LIST;
    } else {
        return -1;
    }
    
    // Check for subcommand
    int start_idx = 2;
    if (argc > 2 && strcmp(argv[2], "set") == 0) {
        args->subcommand = SUBCMD_SET;
        if (argc > 3) {
            args->set_value = atoi(argv[3]);
            start_idx = 4;
        } else {
            fprintf(stderr, "Error: 'set' requires a value\n");
            return -1;
        }
    } else if (argc > 2 && strcmp(argv[2], "restore") == 0) {
        args->subcommand = SUBCMD_RESTORE;
        start_idx = 3;
    } else if (argc > 2 && strcmp(argv[2], "json") == 0) {
        args->subcommand = SUBCMD_JSON;
        start_idx = 3;
    }
    
    static struct option long_options[] = {
        {"device", required_argument, 0, 'd'},
        {"uuid", required_argument, 0, 'u'},
        {"temp-unit", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    optind = start_idx;
    while ((opt = getopt_long(argc, argv, "d:u:t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                args->device_count = parse_device_range(optarg, args->devices, MAX_DEVICES);
                args->all_devices = 0;
                break;
            case 'u':
                strncpy(args->uuid, optarg, sizeof(args->uuid) - 1);
                args->use_uuid = 1;
                args->all_devices = 0;
                break;
            case 't':
                if (strcmp(optarg, "C") == 0 || strcmp(optarg, "c") == 0) {
                    args->temp_unit = TEMP_CELSIUS;
                } else if (strcmp(optarg, "F") == 0 || strcmp(optarg, "f") == 0) {
                    args->temp_unit = TEMP_FAHRENHEIT;
                } else if (strcmp(optarg, "K") == 0 || strcmp(optarg, "k") == 0) {
                    args->temp_unit = TEMP_KELVIN;
                } else {
                    fprintf(stderr, "Error: Invalid temperature unit '%s'\n", optarg);
                    return -1;
                }
                break;
            case 'h':
                return -1;
            default:
                return -1;
        }
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    nvmlReturn_t result;
    cli_args_t args;
    unsigned int device_count;
    
    if (parse_args(argc, argv, &args) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    
    result = nvmlInit();
    if (result != NVML_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize NVML (%s)\n", nvmlErrorString(result));
        return 1;
    }
    
    result = nvmlDeviceGetCount(&device_count);
    if (result != NVML_SUCCESS) {
        fprintf(stderr, "Error: Failed to get device count (%s)\n", nvmlErrorString(result));
        nvmlShutdown();
        return 1;
    }
    
    if (device_count == 0) {
        fprintf(stderr, "No NVIDIA GPUs found\n");
        nvmlShutdown();
        return 1;
    }
    
    // Handle UUID selection
    if (args.use_uuid) {
        int device_id = find_device_by_uuid(args.uuid, device_count);
        if (device_id < 0) {
            fprintf(stderr, "Error: Device with UUID '%s' not found\n", args.uuid);
            nvmlShutdown();
            return 1;
        }
        args.devices[0] = device_id;
        args.device_count = 1;
        args.all_devices = 0;
    }
    
    // Setup device list
    int* target_devices;
    int target_count;
    
    if (args.all_devices) {
        static int all_devs[MAX_DEVICES];
        for (unsigned int i = 0; i < device_count && i < MAX_DEVICES; i++) {
            all_devs[i] = i;
        }
        target_devices = all_devs;
        target_count = device_count;
    } else {
        target_devices = args.devices;
        target_count = args.device_count;
    }
    
    // JSON output header
    if (args.subcommand == SUBCMD_JSON && args.command == CMD_INFO) {
        printf("[\n");
    }
    
    // Execute command for each device
    int error_count = 0;
    for (int i = 0; i < target_count; i++) {
        int device_id = target_devices[i];
        
        if (device_id >= (int)device_count) {
            fprintf(stderr, "Error: Device ID %d not found (available: 0-%d)\n", 
                    device_id, device_count - 1);
            error_count++;
            continue;
        }
        
        nvmlDevice_t device;
        result = nvmlDeviceGetHandleByIndex(device_id, &device);
        if (result != NVML_SUCCESS) {
            fprintf(stderr, "Error: Failed to get device handle for device %d (%s)\n", 
                    device_id, nvmlErrorString(result));
            error_count++;
            continue;
        }
        
        switch (args.command) {
            case CMD_INFO:
                if (args.subcommand == SUBCMD_JSON) {
                    print_device_info_json(device, device_id, args.temp_unit, i == target_count - 1);
                } else {
                    print_device_info_human(device, device_id, args.temp_unit);
                }
                break;
                
            case CMD_POWER:
                if (args.subcommand == SUBCMD_SET) {
                    unsigned int limit_mw = args.set_value * 1000;
                    unsigned int min_limit, max_limit;
                    
                    result = nvmlDeviceGetPowerManagementLimitConstraints(device, &min_limit, &max_limit);
                    if (result != NVML_SUCCESS) {
                        fprintf(stderr, "%d:Error: Cannot get power limit constraints (%s)\n", 
                                device_id, nvmlErrorString(result));
                        error_count++;
                        continue;
                    }
                    
                    if (limit_mw < min_limit || limit_mw > max_limit) {
                        fprintf(stderr, "%d:Error: Power limit %uW outside valid range (%.2f-%.2fW)\n", 
                                device_id, args.set_value, min_limit/1000.0, max_limit/1000.0);
                        error_count++;
                        continue;
                    }
                    
                    result = nvmlDeviceSetPowerManagementLimit(device, limit_mw);
                    if (result == NVML_SUCCESS) {
                        printf("%d:Power limit set to %uW\n", device_id, args.set_value);
                    } else {
                        fprintf(stderr, "%d:Error: Failed to set power limit (%s)\n", 
                                device_id, nvmlErrorString(result));
                        error_count++;
                    }
                } else {
                    print_power_cli(device, device_id, target_count == 1);
                }
                break;
                
            case CMD_FAN:
                if (args.subcommand == SUBCMD_SET) {
                    // Set fan speed to manual mode and adjust speed
                    unsigned int num_fans = 0;
                    result = nvmlDeviceGetNumFans(device, &num_fans);
                    if (result != NVML_SUCCESS) {
                        fprintf(stderr, "%d:Error: Cannot get number of fans (%s)\n", 
                                device_id, nvmlErrorString(result));
                        error_count++;
                        continue;
                    }
                    
                    if (num_fans == 0) {
                        fprintf(stderr, "%d:Error: Device has no controllable fans\n", device_id);
                        error_count++;
                        continue;
                    }
                    
                    if (args.set_value > 100) {
                        fprintf(stderr, "%d:Error: Fan speed must be between 0-100%%\n", device_id);
                        error_count++;
                        continue;
                    }
                    
                    // Set fan speed for all fans on this device
                    int fan_errors = 0;
                    for (unsigned int fan = 0; fan < num_fans; fan++) {
                        result = nvmlDeviceSetFanSpeed_v2(device, fan, args.set_value);
                        if (result != NVML_SUCCESS) {
                            fprintf(stderr, "%d:Fan%u:Error: Failed to set fan speed (%s)\n", 
                                    device_id, fan, nvmlErrorString(result));
                            fan_errors++;
                        } else {
                            printf("%d:Fan%u:Set to %u%%\n", device_id, fan, args.set_value);
                        }
                    }
                    
                    if (fan_errors > 0) {
                        error_count++;
                    } else {
                        printf("%d:Warning: Fan control is now MANUAL - monitor temperatures!\n", device_id);
                        printf("%d:Note: Use 'nvml-tool fan restore -d %d' to restore automatic control\n", 
                               device_id, device_id);
                    }
                } else if (args.subcommand == SUBCMD_RESTORE) {
                    // Restore automatic fan control
                    unsigned int num_fans = 0;
                    result = nvmlDeviceGetNumFans(device, &num_fans);
                    if (result != NVML_SUCCESS) {
                        fprintf(stderr, "%d:Error: Cannot get number of fans (%s)\n", 
                                device_id, nvmlErrorString(result));
                        error_count++;
                        continue;
                    }
                    
                    if (num_fans == 0) {
                        fprintf(stderr, "%d:Error: Device has no controllable fans\n", device_id);
                        error_count++;
                        continue;
                    }
                    
                    // Restore automatic control for all fans
                    int fan_errors = 0;
                    for (unsigned int fan = 0; fan < num_fans; fan++) {
                        result = nvmlDeviceSetDefaultFanSpeed_v2(device, fan);
                        if (result != NVML_SUCCESS) {
                            fprintf(stderr, "%d:Fan%u:Error: Failed to restore automatic control (%s)\n", 
                                    device_id, fan, nvmlErrorString(result));
                            fan_errors++;
                        } else {
                            printf("%d:Fan%u:Restored to automatic control\n", device_id, fan);
                        }
                    }
                    
                    if (fan_errors > 0) {
                        error_count++;
                    } else {
                        printf("%d:All fans restored to automatic temperature-based control\n", device_id);
                    }
                } else {
                    print_fan_cli(device, device_id, target_count == 1);
                }
                break;
                
            case CMD_TEMP:
                print_temp_cli(device, device_id, args.temp_unit, target_count == 1);
                break;
                
            case CMD_STATUS:
                print_status_cli(device, device_id, args.temp_unit);
                break;
                
            case CMD_LIST:
                {
                    char uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
                    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
                    
                    nvmlDeviceGetUUID(device, uuid, sizeof(uuid));
                    nvmlDeviceGetName(device, name, sizeof(name));
                    
                    printf("%d:%s %s\n", device_id, uuid, name);
                }
                break;
                
            default:
                break;
        }
    }
    
    // JSON output footer
    if (args.subcommand == SUBCMD_JSON && args.command == CMD_INFO) {
        printf("]\n");
    }
    
    nvmlShutdown();
    return !!error_count;
}