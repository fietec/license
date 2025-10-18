#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <cwalk.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

typedef struct{
    char path[FILENAME_MAX];
    size_t name_start;
} License;

typedef struct{
    License *items;
    size_t count;
    size_t capacity;
} Licenses;


#define LICENSE_NAME "LICENSE"

#define eprintfn(msg, ...) (fprintf(stderr, "[ERROR] "msg"\n", ##__VA_ARGS__))

#define da_append(xs, x)                                                             \
    do {                                                                             \
        if ((xs)->count >= (xs)->capacity) {                                         \
            if ((xs)->capacity == 0) (xs)->capacity = 256;                           \
            else (xs)->capacity *= 2;                                                \
            (xs)->items = realloc((xs)->items, (xs)->capacity*sizeof(*(xs)->items)); \
        }                                                                            \
                                                                                     \
        (xs)->items[(xs)->count++] = (x);                                            \
    } while (0)

static char program_dir[FILENAME_MAX] = {0};
static char license_dir[FILENAME_MAX] = {0};

char* shift_args(int *argc, char ***argv)
{
    assert(*argc > 0 && "argv: out of bounds\n");
    char *result = **argv;
    *argc -= 1;
    *argv += 1;
    return result;
}

int stricmp(char const *a, char const *b)
{
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
}

bool get_exe_path(char *buffer, size_t buffer_size)
{
#ifdef _WIN32
    GetModuleFileNameA(NULL, buffer, buffer_size);
#elif __linux__
    ssize_t len = readlink("/proc/self/exe", buffer, buffer_size-1);
    if (len == -1) return false;
    buffer[len] = '\0';
#else
    fprintf(stderr, "[ERROR] %s:%d:%s: Unsupported platform!\n", __LINE__, __FILE__, __func__);
    return false;
#endif
    cwk_path_normalize(buffer, buffer, buffer_size);
    return true;
}

bool get_parent_dir(const char *path, char *buffer, size_t buffer_size)
{
    size_t parent_len;
    cwk_path_get_dirname(path, &parent_len);
    if (parent_len == 0 || parent_len >= buffer_size) return false;
    memmove(buffer, path, parent_len);
    buffer[parent_len] = '\0';
    cwk_path_normalize(buffer, buffer, buffer_size);
    return true;
}

void print_usage(const char *program_name, Licenses licenses)
{
    printf("Usage: %s [OPTIONS] <license>\n\n", program_name);
    printf("Licenses:\n");
    for (size_t i=0; i<licenses.count; ++i){
        License license = licenses.items[i];
        printf("  - %s\n", license.path+license.name_start);
    }
    printf("\n");
    printf("Options:\n");
    printf("  -h / --help  print this help message\n");
}

Licenses discover_licenses()
{
    Licenses licenses = {0};
    
    // validate license directory
    cwk_path_join(program_dir, "licenses", license_dir, sizeof(license_dir));
    DIR *dir = opendir(license_dir);
    if (dir == NULL){
        printf("[INFO] License directory does not exist.\n");
        int result = mkdir(license_dir, 0755);
        if (result == -1){
            eprintfn("Failed to create directory '%s'!", license_dir);
        } else{
            printf("[INFO] Successfully created directory '%s'!\n", license_dir);
        }
        return licenses;
    }

    struct dirent *item;
    while ((item = readdir(dir)) != NULL){
        if (item->d_type != DT_REG) continue;
        License license;
        cwk_path_join(license_dir, item->d_name, license.path, sizeof(license.path));
        const char *name;
        cwk_path_get_basename(license.path, &name, NULL);
        license.name_start = name - license.path;
        da_append(&licenses, license); 
    }
    closedir(dir);
    return licenses;
}

bool create_license(char *license)
{
    FILE *f_in = fopen(license, "r");
    if (f_in == NULL){
        eprintfn("License source file no longer exists: '%s'!", license);
        return false;
    }
    FILE *f_out = fopen(LICENSE_NAME, "w");
    if (f_out == NULL){
        eprintfn("Could not open output file: '%s'!", LICENSE_NAME);
        fclose(f_in);
        return false;
    }
    size_t nbytes = 128;
    char buffer[nbytes];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, nbytes, f_in)) > 0){
        if (bytes_read != fwrite(buffer, 1, bytes_read, f_out)){
            eprintfn("Could not write all bytes to output file!");
            fclose(f_in);
            fclose(f_out);
            return false;
        }
    }
    fclose(f_in);
    fclose(f_out);
    return true;
}

int main(int argc, char **argv)
{
    if (!get_exe_path(program_dir, sizeof(program_dir))){
        eprintfn("Could not fetch program directory!");
        return 1;
    }
    get_parent_dir(program_dir, program_dir, sizeof(program_dir));
    // discover licenses
    Licenses licenses = discover_licenses();
    // parse arguments
    const char *program_name = shift_args(&argc, &argv);
    
    if (argc <= 0){
        eprintfn("Missing argument!");
        print_usage(program_name, licenses);
        return 1;
    }
    const char *arg = shift_args(&argc, &argv);
    for (size_t i=0; i<licenses.count; ++i){
        License license = licenses.items[i];
        if (stricmp(arg, license.path+license.name_start) == 0){
            if (!create_license(license.path)){
                eprintfn("Could not create LICENSE file!");
                return 1;
            }
            printf("[INFO] Successfully create license file!\n");
            return 0;
        }
    }
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0){
        print_usage(program_name, licenses);
    } else {
        eprintfn("Unknown license or option: '%s'!", arg);
        print_usage(program_name, licenses);
        return 1;
    }
    return 0;
}
