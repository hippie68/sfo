/* Prints and modifies SFO information of either a PS4 PKG or param.sfo file.
 * Made with info from https://www.psdevwiki.com/ps4/Param.sfo.
 * Get updates and Windows binaries at https://github.com/hippie68/sfo. */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global variables
FILE *file;
long int pkg_offset = 0;

// Default options
int option_decimal = 0;
int option_debug = 0;
int option_search = 0;
int option_verbose = 0;
int option_write = 0;

// Shows usage and help
void show_help(char *program_name) {
  printf(
  "Usage: %s [-dDhvw] file [search] [replace]\n\n"
  "  Prints SFO information of either a PS4 PKG or param.sfo file.\n"
  "  The search string is case-insensitive.\n"
  "  To search and replace, write access must be explicitly enabled (option -w).\n"
  "  Doing so without specifying [replace] will write an empty string\n"
  "  or the integer value 0.\n\n"
  "  Options:\n"
  "    -d  Display integer values as decimal numerals\n"
  "    -D  Print debug information\n"
  "    -h  Display this help info\n"
  "    -v  Increase verbosity\n"
  "    -w  Enable write access\n\n",
  program_name);
}

// Replacement function for byteswap.h's bswap_16
uint16_t bswap_16(uint16_t val) {
  return (val >> 8) | (val << 8);
}

// Replacement function for byteswap.h's bswap_32
uint32_t bswap_32(uint32_t val) {
  val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0x00FF00FF );
  return (val << 16) | (val >> 16);
}

// Converts a string to lowercase
void toupper_string(char *string) {
  while (*string) {
    string[0] = toupper(string[0]);
    string++;
  }
}

// Returns "length" bytes, starting at specified offset
int getbytes(long int offset, int length) {
  int bytes;
  fseek(file, pkg_offset + offset, SEEK_SET);
  fread(&bytes, length, 1, file);
  return bytes;
}

// Finds the param.sfo's offset inside a PKG file
int get_pkg_offset() {
  struct pkg_table_entry {
    uint32_t id;
    uint32_t filename_offset;
    uint32_t flags1;
    uint32_t flags2;
    uint32_t offset;
    uint32_t size;
    uint64_t padding;
  } pkg_table_entry;
  int pkg_table_offset = bswap_32(getbytes(0x018, 4));
  int pkg_file_count = bswap_32(getbytes(0x00C, 4));
  fseek(file, pkg_table_offset, SEEK_SET);
  for (int i = 0; i < pkg_file_count; i++) {
    fread(&pkg_table_entry, sizeof(struct pkg_table_entry), 1, file);
    if (bswap_32(pkg_table_entry.id) == 4096 ) { // param.sfo ID
      return bswap_32(pkg_table_entry.offset);
    }
  }
  printf("ERROR: Could not find param.sfo inside PKG file.\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  // Parse command line options
  char *filename, *search, *replace;
  {
    char *arg[3] = {"", "", ""};
    if (argc > 1) {
      int j = 0;
      for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
          for (int o = 1; argv[i][o] != '\0'; o++) {
            switch (argv[i][o]) {
              case 'd':
                option_decimal = 1;
                break;
              case 'D':
                option_debug = 1;
                break;
              case 'h':
                show_help(argv[0]);
                break;
              case 'v':
                option_verbose = 1;
                break;
              case 'w':
                option_write = 1;
                break;
              default:
                printf("ERROR: Unknown option: %c\n", argv[i][o]);
            }
          }
          argv[i][0] = 0;
        } else {
          arg[j++] = argv[i];
        }
      }
    } else {
      show_help(argv[0]);
      return 1;
    }
    filename = arg[0];
    if (strlen(arg[1]) > 0) {
      search = arg[1];
      toupper_string(search);
      option_search = 1;
    }
    replace = arg[2];

    // DEBUG: Print all command line arguments and options
    if (option_debug == 1) {
      for (int i = 0; i < sizeof(arg) / sizeof(arg[0]); i++) {
        printf("DEBUG: arg[%d]: %s\n", i, arg[i]);
      }
      printf("DEBUG: search: %s\n", search);
      printf("DEBUG: replace: %s\n", replace);
      printf("DEBUG: option_decimal: %d\n", option_decimal);
      printf("DEBUG: option_debug: %d\n", option_debug);
      printf("DEBUG: Option search: %d\n", option_search);
      printf("DEBUG: option_verbose: %d\n", option_verbose);
      printf("DEBUG: Option write: %d\n", option_write);
    }
  }

  // Open binary file
  if (option_write == 1) {
    file = fopen(filename, "r+b"); // Read/write
  } else {
    file = fopen(filename, "rb"); // Read only
  }
  if (file == NULL) {
    fprintf(stderr, "ERROR: Could not open file \"%s\".\n", filename);
    return 1;
  }

  // Set PKG offset if file is a PKG
  int magic = getbytes(0, 4);
  if (magic == 1414415231) { // PKG file
    pkg_offset = get_pkg_offset();
    magic = getbytes(0, 4);
  }

  // Exit if param.sfo not found
  if (magic != 1179865088) { // Param.sfo file
    fprintf(stderr, "ERROR: Param.sfo magic byte not found.\n");
    return 1;
  }

  // Load param.sfo header
  struct sfo_header {
    uint32_t magic;
    uint32_t version;
    uint32_t keytable_offset;
    uint32_t datatable_offset;
    uint32_t indextable_entries;
  } sfo_header;
  fseek(file, pkg_offset, SEEK_SET);
  fread(&sfo_header, sizeof(struct sfo_header), 1, file);

  // Print header info if verbose
  if (option_verbose == 1) {
    char *version = malloc(9);
    sprintf(version, "%X", sfo_header.version);
    printf("Param.sfo version: %c.%c%c\nNumber of parameters: %d\n",
      version[0], version[1], version[2], sfo_header.indextable_entries);
  }

  // Define the index table
  const int indextable_offset = 0x14;
  const int indextable_entry_size = 0x10;
  struct indextable_entry {
    uint16_t keytable_offset;
    uint16_t param_fmt; // Type of data
    uint32_t parameter_length;
    uint32_t parameter_max_length;
    uint32_t datatable_offset;
  } indextable_entry[sfo_header.indextable_entries];

  // Loop starts here
  int integer = 0;
  for (int i = 0; i < sfo_header.indextable_entries; i++) {
    fseek(file, pkg_offset + indextable_offset + i * indextable_entry_size, SEEK_SET);
    fread(&indextable_entry[i], sizeof(struct indextable_entry), 1, file);

    // Get current parameter's key name
    char *key;
    key = malloc(100);
    fseek(file, pkg_offset + sfo_header.keytable_offset + indextable_entry[i].keytable_offset, SEEK_SET);
    fread(key, 100, 1, file);

    // Continue loop if key does not match the (optional) search term
    if (option_search == 0) {
      // Print current parameter's index
      if (option_verbose == 1) {
        printf("[%02d] ", i);
      }
      // Print current parameter's key
      printf("%s=", key);
    } else if (strcmp(search, key) != 0) {
      continue;
    }

    // Write
    if (option_write == 1) {
      if (strcmp(search, key) == 0) {
        switch (indextable_entry[i].param_fmt) {
          case 516: // UTF-8 string
            // Check new string for valid length
            if (strlen(replace) > indextable_entry[i].parameter_max_length - 1) {
              fprintf(stderr, "ERROR: Replacement string \"%s\" too large (%d characters allowed).\n", replace, indextable_entry[i].parameter_max_length - 1);
              return 0;
            }
            // Fill space with zeros
            char *string;
            string = malloc(indextable_entry[i].parameter_max_length);
            memset(string, '\0', indextable_entry[i].parameter_max_length);
            fseek(file, pkg_offset + sfo_header.datatable_offset + indextable_entry[i].datatable_offset, SEEK_SET);
            fwrite(string, indextable_entry[i].parameter_max_length, 1, file);
            free(string);
            // Write new string to file
            fseek(file, pkg_offset + sfo_header.datatable_offset + indextable_entry[i].datatable_offset, SEEK_SET);
            fwrite(replace, strlen(replace), 1, file);
            // Write new parameter length to file
            indextable_entry[i].parameter_length = strlen(replace) + 1;
            fseek(file, pkg_offset + indextable_offset + i * indextable_entry_size, SEEK_SET);
            fwrite(&indextable_entry[i], indextable_entry_size, 1, file);
            break;
          case 1028: // Integer
            fseek(file, pkg_offset + sfo_header.datatable_offset + indextable_entry[i].datatable_offset, SEEK_SET);
            uint32_t temp;
            if (strlen(replace) > 1 && (replace[1] == 'x' || replace[1] == 'X')) {
              sscanf(replace, "%x", &temp);
            } else {
              temp = atoi(replace);
            }
            fwrite(&temp, 4, 1, file);
            break;
          default:
            fprintf(stderr, "ERROR: Unknown data type: %04X\n", bswap_16(indextable_entry[i].param_fmt));
            return 1;
        }
        fclose(file); // Writes the buffer
        return 0;
      }
    // Read
    } else {
      switch (indextable_entry[i].param_fmt) {
        case 516: // UTF-8 string
          fseek(file, pkg_offset + sfo_header.datatable_offset + indextable_entry[i].datatable_offset, SEEK_SET);
          char *string;
          string = malloc(indextable_entry[i].parameter_length);
          fread(string, indextable_entry[i].parameter_length, 1, file);
          if (option_verbose == 1) {
            printf("\"%s\" (%d/%d bytes)\n", string, indextable_entry[i].parameter_length, indextable_entry[i].parameter_max_length);
          } else {
            printf("%s\n", string);
          }
          free(string);
          break;
        case 1028: // Integer
          fseek(file, pkg_offset + sfo_header.datatable_offset + indextable_entry[i].datatable_offset, SEEK_SET);
          fread(&integer, 4, 1, file);
          if (option_decimal == 1) {
            if (option_verbose == 1) {
              printf("%d (%d bytes)\n", integer, indextable_entry[i].parameter_length);
            } else {
              printf("%d\n", integer);
            }
          } else {
            if (option_verbose == 1) {
              printf("0x%08X (%d bytes)\n", integer, indextable_entry[i].parameter_length);
            } else {
              printf("0x%08x\n", integer);
            }
          }
          break;
        default:
          fprintf(stderr, "ERROR: Unknown data type: %04X\n", bswap_16(indextable_entry[i].param_fmt));
      }
    }
    free(key);
  }

  fclose(file);
  return 0;
}
