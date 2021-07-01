/* Prints SFO information of either a PS4 PKG or param.sfo file.
 * Made with info from https://www.psdevwiki.com/ps4/Param.sfo.
 * Get updates and Windows binaries at https://github.com/hippie68/sfo. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void show_help(char *program_name) {
  printf(
  "Usage: %s file [search]\n"
  "Prints SFO information of either a PS4 PKG or param.sfo file.\n"
  "Providing a search string will output the value of that specific key only.\n",
  program_name);
}

FILE *file;
long long int pkg_offset = 0;

// Replacement function for byteswap.h's bswap_16
uint16_t bswap_16(uint16_t val) {
  return (val >> 8) | (val << 8);
}

// Replacement function for byteswap.h's bswap_32
uint32_t bswap_32(uint32_t val) {
  val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0x00FF00FF );
  return (val << 16) | (val >> 16);
}

// Returns "length" bytes, starting at specified offset
int getbytes(long long int offset, int length) {
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
  int i;
  int pkg_file_count = bswap_32(getbytes(0x00C, 4));
  int pkg_table_offset = bswap_32(getbytes(0x018, 4));

  fseek(file, pkg_table_offset, SEEK_SET);
  for (i = 0; i < pkg_file_count; i++) {
    fseek(file, 32, SEEK_CUR);
    fread(&pkg_table_entry, sizeof(struct pkg_table_entry), 1, file);
    if (bswap_32(pkg_table_entry.id) == 4096 ) { // param.sfo ID
      return bswap_32(pkg_table_entry.offset);
    }
  }
  printf("Could not find param.sfo inside PKG file.");
  exit(1);
}

int main(int argc, char *argv[])
{
  char *filename;
  if (argc > 1) {
    filename = argv[1];
  } else {
    show_help(argv[0]);
    return 1;
  }

  // Open binary file
  file = fopen(filename, "rb");
  if (file == NULL) {
    fprintf(stderr, "ERROR: Could not open file \"%s\".", filename);
    return 1;
  }

  // Set PKG offset if file is a PKG
  int magic = bswap_32(getbytes(0, 4));
  if (magic == 2135117396) { // PKG file
    pkg_offset = get_pkg_offset();
    magic = bswap_32(getbytes(0, 4));
  }

  // Exit if param.sfo not found
  if (magic != 5264198) {
    fprintf(stderr, "ERROR: param.sfo magic not found.\n");
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

  // Loop through the index table
  struct indextable_entry {
    uint16_t keytable_offset;
    uint16_t param_fmt; // Type of data
    uint32_t parameter_length;
    uint32_t parameter_max_length;
    uint32_t datatable_offset;
  } indextable_entry[sfo_header.indextable_entries];
  int i, integer = 0;
  for (i = 0; i < sfo_header.indextable_entries; i++) {
    fseek(file, pkg_offset + 0x14 + i * 0x10, SEEK_SET);
    fread(&indextable_entry[i], sizeof(struct indextable_entry), 1, file);

    // Get key name
    char *key;
    key = malloc(100);
    fseek(file, pkg_offset + sfo_header.keytable_offset + indextable_entry[i].keytable_offset, SEEK_SET);
    fread(key, 100, 1, file);

    // Get data
    fseek(file, pkg_offset + sfo_header.datatable_offset + indextable_entry[i].datatable_offset, SEEK_SET);
    switch (indextable_entry[i].param_fmt) {
      case 516: // UTF-8 string
        ; // Empty statement required by the C standard
        char *string;
        string = malloc(indextable_entry[i].parameter_length);
        fread(string, (indextable_entry[i].parameter_length), 1, file);
        printf("%s=%s\n", key, string);
        free(string);
        break;
      case 1028: // Integer
        fread(&integer, 4, 1, file);
        printf("%s=0x%08x\n", key, integer);
        break;
      default:
        printf("[UNKNOWN DATA TYPE: %04X]\n", bswap_16(indextable_entry[i].param_fmt));
    }
    free(key);
  }

  // Close the file
  fclose(file);
}
