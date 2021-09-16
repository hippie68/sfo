/* Prints or modifies SFO parameters of a PS4 PKG or a param.sfo file.
 * Made with info from https://www.psdevwiki.com/ps4/Param.sfo.
 * Get updates and Windows binaries at https://github.com/hippie68/sfo. */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global variables
FILE *file;
long int psf_offset = 0;

// Default options
int option_decimal = 0;
int option_search = 0;
int option_verbose = 0;
int option_write = 0;

// Returns a filename without its path
char *basename(char *filename) {
  #if defined(_WIN32) || defined(_WIN64)
  char *base = strrchr(filename, '\\');
  #else
  char *base = strrchr(filename, '/');
  #endif
  if (base != NULL && strlen(base) > 1) {
    base++;
    return base;
  } else {
    return filename;
  }
}

// Shows usage and help
void show_help(char *program_name) {
  printf(
  "Usage: %s [-dhvw] file [search] [replace]\n\n"
  "  Prints or modifies SFO parameters of a PS4 PKG or a param.sfo file.\n"
  "  The [search] string is case-insensitive.\n"
  "  To modify parameters, write access must be explicitly enabled (option -w).\n"
  "  Enabling write access without specifying a [replace] string will write\n"
  "  an empty string or the integer value \"0\".\n\n"
  "  Options:\n"
  "    -d  Display integer values as decimal numerals\n"
  "    -h  Display this help info\n"
  "    -v  Increase verbosity\n"
  "    -w  Enable write access\n",
  basename(program_name));
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

// Converts a string to uppercase
void toupper_string(char *string) {
  while (*string) {
    string[0] = toupper(string[0]);
    string++;
  }
}

// Reads and returns a 32-bit integer, starting at specified file offset
uint32_t get_int(long int offset) {
  uint32_t integer;
  fseek(file, psf_offset + offset, SEEK_SET);
  fread(&integer, sizeof integer, 1, file);
  return integer;
}

// Finds the param.sfo's offset inside a PS4 PKG file
int get_ps4_pkg_offset() {
  const uint32_t pkg_table_offset = bswap_32(get_int(0x018));
  const uint32_t pkg_file_count = bswap_32(get_int(0x00C));
  struct pkg_table_entry {
    uint32_t id;
    uint32_t filename_offset;
    uint32_t flags1;
    uint32_t flags2;
    uint32_t offset;
    uint32_t size;
    uint64_t padding;
  } pkg_table_entry[pkg_file_count];
  fseek(file, pkg_table_offset, SEEK_SET);
  fread(pkg_table_entry, sizeof (struct pkg_table_entry) * pkg_file_count, 1, file);
  for (int i = 0; i < pkg_file_count; i++) {
    if (pkg_table_entry[i].id == 1048576) { // param.sfo ID
      return bswap_32(pkg_table_entry[i].offset);
    }
  }
  fprintf(stderr, "ERROR: Could not find a param.sfo file inside the PS4 PKG.\n");
  fclose(file);
  exit(1);
}

int main(int argc, char *argv[])
{
  char *filename, *search, *replace;

  // Parse command line arguments
  {
    char *arg[3] = {"", "", ""}; // Non-option arguments
    if (argc > 1) {
      for (int i = 1, j = 0; i < argc; i++) {
        if (argv[i][0] == '-') {
          for (int o = 1; argv[i][o] != '\0'; o++) {
            switch (argv[i][o]) {
              case 'd':
                option_decimal = 1;
                break;
              case 'h':
                show_help(argv[0]);
                return 0;
                break;
              case 'v':
                option_verbose = 1;
                break;
              case 'w':
                option_write = 1;
                break;
              default:
                fprintf(stderr, "ERROR: Unknown option: %c\n", argv[i][o]);
                return 1;
            }
          }
        } else {
          // Track non-option arguments
          arg[j++] = argv[i];
        }
      }
    } else {
      show_help(argv[0]);
      return 1;
    }

    // Assign non-option arguments
    filename = arg[0];
    search = arg[1];
    if (strlen(search) > 0) {
      toupper_string(search);
      option_search = 1;
    }
    replace = arg[2];

    // Sanity checks for enabled write access
    if (option_write == 0 && strlen(replace) > 0) {
      fprintf(stderr, "ERROR: You specified [replace] without enabling write access (option -w).\n\n");
      show_help(argv[0]);
      return 1;
    }
    if (option_write == 1 && strlen(search) == 0) {
      fprintf(stderr, "ERROR: You enabled write access (option -w) without specifying a search string.\n\n");
      show_help(argv[0]);
      return 1;
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
  int magic = get_int(0);
  if (magic == 1414415231) { // PS4 PKG file
    psf_offset = get_ps4_pkg_offset();
    magic = get_int(0);
  } else if (magic == 1128612691) { // Disc param.sfo
    psf_offset = 0x800;
    magic = get_int(0);
  }

  // Exit if param.sfo not found
  if (magic != 1179865088) { // Param.sfo file
    fprintf(stderr, "ERROR: Param.sfo magic number not found.\n");
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
  fseek(file, psf_offset, SEEK_SET);
  fread(&sfo_header, sizeof (struct sfo_header), 1, file);

  // Print header info if verbose
  if (option_verbose == 1) {
    char *version = malloc(9);
    sprintf(version, "%X", sfo_header.version);
    printf("Param.sfo version: %c.%c%c\nNumber of parameters: %d\n",
      version[0], version[1], version[2], sfo_header.indextable_entries);
  }

  // Define the index table
  const long int indextable_offset = 0x14;
  const long int indextable_entry_size = 0x10;
  struct indextable_entry {
    uint16_t keytable_offset;
    uint16_t param_fmt; // Type of data
    uint32_t parameter_length;
    uint32_t parameter_max_length;
    uint32_t datatable_offset;
  } indextable_entry[sfo_header.indextable_entries];

  // Loop starts here
  for (int i = 0; i < sfo_header.indextable_entries; i++) {
    fseek(file, psf_offset + indextable_offset + i * indextable_entry_size, SEEK_SET);
    fread(&indextable_entry[i], sizeof (struct indextable_entry), 1, file);

    // Get current parameter's key name
    char key[50];
    fseek(file, psf_offset + sfo_header.keytable_offset + indextable_entry[i].keytable_offset, SEEK_SET);
    fread(key, 50, 1, file);
    key[49] = '\0';

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
            ;
            // Check new string for valid length
            int replace_length = strlen(replace);
            if (replace_length > indextable_entry[i].parameter_max_length - 1) {
              fprintf(stderr, "ERROR: Replacement string \"%s\" too large (%d/%d characters).\n", replace, replace_length, indextable_entry[i].parameter_max_length - 1);
              return 0;
            }
            // Write new string, zero-padded, to file
            {
              char string[indextable_entry[i].parameter_max_length];
              memset(string, 0, sizeof string);
              memcpy(string, replace, replace_length);
              fseek(file, psf_offset + sfo_header.datatable_offset + indextable_entry[i].datatable_offset, SEEK_SET);
              fwrite(string, sizeof string, 1, file);
            }
            // Write new parameter length to file
            indextable_entry[i].parameter_length = replace_length + 1;
            fseek(file, psf_offset + indextable_offset + i * indextable_entry_size, SEEK_SET);
            fwrite(&indextable_entry[i], indextable_entry_size, 1, file);
            break;
          case 1028: // Integer
            fseek(file, psf_offset + sfo_header.datatable_offset + indextable_entry[i].datatable_offset, SEEK_SET);
            uint32_t integer;
            if (strlen(replace) > 1 && (replace[1] == 'x' || replace[1] == 'X')) {
              sscanf(replace, "%x", &integer);
            } else {
              integer = atoi(replace);
            }
            fwrite(&integer, sizeof integer, 1, file);
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
          {
            char string[indextable_entry[i].parameter_length];
            fseek(file, psf_offset + sfo_header.datatable_offset + indextable_entry[i].datatable_offset, SEEK_SET);
            fread(string, indextable_entry[i].parameter_length, 1, file);
            string[indextable_entry[i].parameter_length - 1] = '\0';
            if (option_verbose == 1) {
              printf("\"%s\" (%d/%d bytes UTF-8 string)\n", string, indextable_entry[i].parameter_length, indextable_entry[i].parameter_max_length);
            } else {
              printf("%s\n", string);
            }
          }
          break;
        case 1028: // Integer
          ;
          uint32_t integer;
          fseek(file, psf_offset + sfo_header.datatable_offset + indextable_entry[i].datatable_offset, SEEK_SET);
          fread(&integer, sizeof integer, 1, file);
          if (option_decimal == 1) {
            if (option_verbose == 1) {
              printf("%d (%d bytes unsigned integer)\n", integer, indextable_entry[i].parameter_length);
            } else {
              printf("%d\n", integer);
            }
          } else {
            if (option_verbose == 1) {
              printf("0x%08X (%d bytes unsigned integer)\n", integer, indextable_entry[i].parameter_length);
            } else {
              printf("0x%08X\n", integer);
            }
          }
          break;
        default:
          fprintf(stderr, "ERROR: Unknown data type: %04X\n", bswap_16(indextable_entry[i].param_fmt));
      }
    }
  }

  fclose(file);
  return 0;
}
