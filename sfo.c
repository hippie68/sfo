/* Reads a file to print or modify its SFO parameters.
 * Supported file types:
 *   - PS4 param.sfo (print and modify)
 *   - PS4 disc param.sfo (print only)
 *   - PS4 PKG (print only)
 * Made with info from https://www.psdevwiki.com/ps4/Param.sfo.
 * Get updates and Windows binaries at https://github.com/hippie68/sfo. */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if __has_include("<byteswap.h>")
#include <byteswap.h>
#else
// Replacement function for byteswap.h's bswap_32
uint32_t bswap_32(uint32_t val) {
  val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0x00FF00FF );
  return (val << 16) | (val >> 16);
}
#endif

// Global variables
const char program_version[] = "1.02 (January 4, 2022)";
char *program_name;
char *query_string;
FILE *file;
int option_debug;
int option_decimal;
int option_force;
int option_new_file;
int option_verbose;

// Complete param.sfo file structure, 4 parts:
// 1. header
// 2. all entries
// 3. key_table.content (with trailing 4-byte alignment)
// 4. data_table.content

struct header {
  uint32_t magic;
  uint32_t version;
  uint32_t key_table_offset;
  uint32_t data_table_offset;
  uint32_t entries_count;
} header;

struct index_table_entry {
  uint16_t key_offset;
  uint16_t param_fmt;
  uint32_t param_len;
  uint32_t param_max_len;
  uint32_t data_offset;
} *entries;

struct table {
  unsigned int size;
  char *content;
} key_table, data_table;

enum cmd {cmd_add, cmd_delete, cmd_edit, cmd_set};

struct command {
  enum cmd cmd;
  struct {
    char *type;
    char *key;
    char *value;
  } param;
} *commands;
int commands_count;

void load_header(FILE *file) {
  if (fread(&header, sizeof(struct header), 1, file) != 1) {
    fprintf(stderr, "Could not read header.\n");
    exit(1);
  }
}

void load_entries(FILE *file) {
  unsigned int size = sizeof(struct index_table_entry) * header.entries_count;
  entries = malloc(size);
  if (entries == NULL) {
    fprintf(stderr, "Could not allocate %u bytes of memory for index table.\n",
      size);
    exit(1);
  }
  if (size && fread(entries, size, 1, file) != 1) {
    fprintf(stderr, "Could not read index table entries.\n");
    exit(1);
  }
}

void load_key_table(FILE *file) {
  key_table.size = header.data_table_offset - header.key_table_offset;
  key_table.content = malloc(key_table.size);
  if (key_table.content == NULL) {
    fprintf(stderr, "Could not allocate %u bytes of memory for key table.\n",
      key_table.size);
    exit(1);
  }
  if (key_table.size && fread(key_table.content, key_table.size, 1, file) != 1) {
    fprintf(stderr, "Could not read key table.\n");
    exit(1);
  }
}

void load_data_table(FILE *file) {
  if (header.entries_count) {
    data_table.size =
      (entries[header.entries_count - 1].data_offset +
      entries[header.entries_count - 1].param_max_len);
  } else {
    data_table.size = 0; // For newly created, empty param.sfo files
  }
  data_table.content = malloc(data_table.size);
  if (data_table.content == NULL) {
    fprintf(stderr, "Could not allocate %u bytes of memory for data table.\n",
      data_table.size);
    exit(1);
  }
  if (data_table.size && fread(data_table.content, data_table.size, 1, file) != 1) {
    fprintf(stderr, "Could not read data table.\n");
    exit(1);
  }
}

// Debug function that prints a byte array's content in hex editor style
void hexprint(char *array, int array_len) {
  int offset = 0;
  fprintf(stderr, "      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  while (offset < array_len) {
    // Print starting offset
    fprintf(stderr, "%04x ", offset);
    // Print bytes
    for (int i = 0; i < 16 && offset + i < array_len; i++) {
      fprintf(stderr, "%02x ", array[offset + i]);
    }
    int remaining_bytes = array_len - offset;
    if (remaining_bytes < 16) {
      for (int i = 0; i < 16 - remaining_bytes; i++) {
        fprintf(stderr, "   ");
      }
    }
    // Print characters
    for (int i = 0; i < 16 && offset + i < array_len; i++) {
      if (isprint(array[offset + i])) {
        fprintf(stderr, "%c", array[offset + i]);
      } else {
        fprintf(stderr, ".");
      }
    }
    fprintf(stderr, "\n");
    offset += 16;
  }
  if (array_len > 64) fprintf(stderr, "      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
}

// Debug function
void print_header(void) {
  fprintf(stderr, "Header:\n");
  fprintf(stderr, "Size: %d\n", sizeof(header));
  fprintf(stderr, ".magic: %u\n", header.magic);
  fprintf(stderr, ".version: %u\n", header.version);
  fprintf(stderr, ".key_table_offset: %u\n", header.key_table_offset);
  fprintf(stderr, ".data_table_offset: %u\n", header.data_table_offset);
  fprintf(stderr, ".entries_count: %u\n", header.entries_count);
  fprintf(stderr, "\n");
}

// Debug function
void print_entries(void) {
  fprintf(stderr, "Index table:\n");
  fprintf(stderr, "Size: %d\n", sizeof(struct index_table_entry) * header.entries_count);
  for (int i = 0; i < header.entries_count; i++) {
    fprintf(stderr, "Entry %d:\n", i);
    fprintf(stderr, "  .key_offset: %u -> \"%s\"\n", entries[i].key_offset,
      &key_table.content[entries[i].key_offset]);
    fprintf(stderr, "  .param_fmt: %u\n", entries[i].param_fmt);
    fprintf(stderr, "  .param_len: %u\n", entries[i].param_len);
    fprintf(stderr, "  .param_max_len: %u\n", entries[i].param_max_len);
    fprintf(stderr, "  .data_offset: %u (0x%x)-> ", entries[i].data_offset, entries[i].data_offset);
    switch (entries[i].param_fmt) {
      case 516:
      case 1024:
        fprintf(stderr, "\"%s\"\n", &data_table.content[entries[i].data_offset]);
        break;
      case 1028:
        ;
        uint32_t *integer = (uint32_t *) &data_table.content[entries[i].data_offset];
        fprintf(stderr, "0x%08x\n", *integer);
        break;
    }
  }
  fprintf(stderr, "\n");
}

// Debug function
void print_key_table(void) {
  fprintf(stderr, "Key table:\n");
  fprintf(stderr, "Size: %d\n", key_table.size);
  if (key_table.size) {
    fprintf(stderr, "Content:\n");
    for (int i = 0; i < key_table.size; i++) {
      if (isprint(key_table.content[i])) {
        fprintf(stderr, "%c", key_table.content[i]);
      } else {
        fprintf(stderr, "'\\%d'", key_table.content[i]);
      }
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
}

// Debug function
void print_data_table(void) {
  fprintf(stderr, "Data table:\n");
  fprintf(stderr, "Size: %d (0x%x)\n", data_table.size, data_table.size);
  if (data_table.size) {
    fprintf(stderr, "Content:\n");
    hexprint(data_table.content, data_table.size);
  }
  fprintf(stderr, "\n");
}

// Saves all 4 param.sfo parts to a param.sfo file
void save_to_file(char *file_name) {
  FILE *file = fopen(file_name, "wb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\" in write mode.\n", file_name);
    exit(1);
  }

  // Adjust header's table offsets before saving
  header.key_table_offset = sizeof(struct header) +
    sizeof(struct index_table_entry) * header.entries_count;
  header.data_table_offset = header.key_table_offset + key_table.size;

  if (fwrite(&header, sizeof(struct header), 1, file) != 1) {
    fprintf(stderr, "Could not write header to file \"%s\".\n", file_name);
    exit(1);
  }
  if (header.entries_count && fwrite(entries,
    sizeof(struct index_table_entry) * header.entries_count, 1, file) != 1) {
    fprintf(stderr, "Could not write index table to file \"%s\".\n", file_name);
    exit(1);
  }
  if (key_table.size && fwrite(key_table.content, key_table.size, 1, file) != 1) {
    fprintf(stderr, "Could not write key table to file \"%s\".\n", file_name);
    exit(1);
  }
  if (data_table.size && fwrite(data_table.content, data_table.size, 1, file) != 1) {
    fprintf(stderr, "Could not write data table to file \"%s\".\n", file_name);
    exit(1);
  }

  fclose(file);
}

// Prints a single parameter
int print_param(char *key) {
  for (int i = 0; i < header.entries_count; i ++) {
    if (!strcmp(key, &key_table.content[entries[i].key_offset])) {
      switch(entries[i].param_fmt) {
        case 516:
        case 1024:
          printf("%s\n", &data_table.content[entries[i].data_offset]);
          return 0;
        case 1028:
          ;
          uint32_t *integer = (uint32_t *) &data_table.content[entries[i].data_offset];
          if (option_decimal) {
            printf("%u\n", *integer);
          } else {
            printf("0x%08x\n", *integer);
          }
          return 0;
      }
    }
  }
  return 1; // Parameter not found
}

// Prints all parameters
void print_params(void) {
  uint32_t *integer;
  if (option_verbose) {
    char version[6] = {0};
    snprintf(version, 6, "%04x", header.version);
    version[4] = version[3];
    version[3] = version[2];
    version[2] = '.';
    if (version[0] == '0') {
      printf("Param.sfo version: %s\n", &version[1]);
    } else {
      printf("Param.sfo version: %s\n", version);
    }
    printf("Number of parameters: %d\n", header.entries_count);
  }
  for (int i = 0; i < header.entries_count; i++) {
    switch (entries[i].param_fmt) {
      case 516:
        if (option_verbose) {
          printf("[%d] %s=\"%s\" (%d/%d bytes UTF-8 string)\n", i,
            &key_table.content[entries[i].key_offset],
            &data_table.content[entries[i].data_offset],
            entries[i].param_len, entries[i].param_max_len);
        } else {
          printf("%s=%s\n", &key_table.content[entries[i].key_offset],
            &data_table.content[entries[i].data_offset]);
        }
        break;
      case 1024:
        if (option_verbose) {
          printf("[%d] %s=\"%s\" (%d/%d bytes UTF-8 special mode string)\n", i,
            &key_table.content[entries[i].key_offset],
            &data_table.content[entries[i].data_offset],
            entries[i].param_len, entries[i].param_max_len);
        } else {
          printf("%s=%s\n", &key_table.content[entries[i].key_offset],
            &data_table.content[entries[i].data_offset]);
        }
        break;
      case 1028:
        integer = (uint32_t *) &data_table.content[entries[i].data_offset];
        if (option_verbose) {
          if (option_decimal) {
            printf("[%d] %s=%u (%d/%d bytes unsigned integer)\n", i,
              &key_table.content[entries[i].key_offset], *integer,
              entries[i].param_len, entries[i].param_max_len);
          } else {
            printf("[%d] %s=0x%08x (%d/%d bytes unsigned integer)\n", i,
              &key_table.content[entries[i].key_offset], *integer,
              entries[i].param_len, entries[i].param_max_len);
          }
        } else {
          if (option_decimal) {
            printf("%s=%u\n", &key_table.content[entries[i].key_offset], *integer);
          } else {
            printf("%s=0x%08x\n", &key_table.content[entries[i].key_offset], *integer);
          }
        }
        break;
    }
  }
}

// Replacement for realloc() that exits on error
static inline void *_realloc(void *ptr, unsigned int size) {
  if (size == 0) { // Avoid double free (which is implementation-dependant)
    if (ptr) free(ptr);
    ptr = NULL;
  } else if ((ptr = realloc(ptr, size)) == NULL) {
    fprintf(stderr, "Failed to reallocate memory.\n");
    exit(1);
  }
  return ptr;
}

// Resizes the data table, starting at specified offset
void expand_data_table(int offset, int additional_size) {
  data_table.size += additional_size;
  data_table.content = _realloc(data_table.content, data_table.size);
  // Move higher indexed data to make room for new data
  for (int i = data_table.size - 1; i >= offset + additional_size; i--) {
    data_table.content[i] = data_table.content[i - additional_size];
  }
  // Set new memory to zero
  memset(&data_table.content[offset], 0, additional_size);
}

// Returns a parameter's index table position
int get_index(char *key) {
  for (int i = 0; i < header.entries_count; i++) {
    if (strcmp(key, &key_table.content[entries[i].key_offset]) == 0) {
      return i;
    }
  }
  return -1;
}

// Edits a parameter in memory
void edit_param(char *key, char *value, int no_fail) {
  int index = get_index(key);
  if (index < 0) { // Parameter not found
    if (no_fail) {
      return;
    } else {
      fprintf(stderr, "Could not edit \"%s\": parameter not found.\n", key);
      exit(1);
    }
  }

  switch (entries[index].param_fmt) {
    case 516: // String
    case 1024: // Special mode string
      entries[index].param_len = strlen(value) + 1;
      // Enlarge data table if new string is longer than allowed
      int diff = entries[index].param_len - entries[index].param_max_len;
      if (diff > 0) {
        int offset = entries[index].data_offset + entries[index].param_max_len;
        entries[index].param_max_len = entries[index].param_len;

        // 4-byte alignment
        while (entries[index].param_max_len % 4) {
          entries[index].param_max_len++;
          diff++;
        }

        expand_data_table(offset, diff);

        // Adjust follow-up index table entries' data offsets
        for (int i = index + 1; i < header.entries_count; i++) {
          entries[i].data_offset += diff;
        }
      }
      // Overwrite old data with zeros
      memset(&data_table.content[entries[index].data_offset], 0,
        entries[index].param_max_len);
      // Save new string to data table
      snprintf(&data_table.content[entries[index].data_offset],
        entries[index].param_max_len, "%s", value);
      break;
    case 1028: // Integer
      ;
      uint32_t integer = strtoul(value, NULL, 0);
      memcpy(&data_table.content[entries[index].data_offset], &integer, 4);
      break;
  }
}

// Pad a table to obey the 4-byte alignment rule
// Currently only used for the key table
void pad_table(struct table *table) {
  // Remove all trailing zeros
  while (table->size > 0 && table->content[table->size - 1] == '\0') {
    table->size--;
  }
  if (table->size) table->size++; // Re-add 1 zero if there are strings left

  table->content = _realloc(table->content, table->size);
  // Pad table with zeros
  while (table->size % 4) {
    table->size++;
    table->content = _realloc(table->content, table->size);
    table->content[table->size - 1] = '\0';
  }
}

// Deletes a parameter from memory
void delete_param(char *key, int no_fail) {
  int index = get_index(key);
  if (index < 0) { // Parameter not found
    if (no_fail) {
      return;
    } else {
      fprintf(stderr, "Could not delete \"%s\": parameter not found.\n", key);
      exit(1);
    }
  }

  // Delete parameter from key table
  for (int i = entries[index].key_offset; i < key_table.size - strlen(key) - 1; i++) {
    key_table.content[i] = key_table.content[i + strlen(key) + 1];
  }

  // Resize key table
  key_table.size -= strlen(key) + 1;
  key_table.content = _realloc(key_table.content, key_table.size);
  pad_table(&key_table);

  // Delete parameter from data table
  for (int i = entries[index].data_offset; i < data_table.size - entries[index].param_max_len; i++) {
    data_table.content[i] = data_table.content[i + entries[index].param_max_len];
  }

  // Resize data table
  data_table.size -= entries[index].param_max_len;
  if (data_table.size) {
    data_table.content = _realloc(data_table.content, data_table.size);
  } else {
    free(data_table.content);
    data_table.content = NULL;
  }

  // Delete parameter from index table
  int param_max_len = entries[index].param_max_len;
  for (int i = index; i < header.entries_count - 1; i++) {
    entries[i] = entries[i + 1];
    entries[i].key_offset -= strlen(key) + 1;
    entries[i].data_offset -= param_max_len;
  }

  // Resize index table
  header.entries_count--;
  if (header.entries_count) {
    entries = _realloc(entries,
      sizeof(struct index_table_entry) * header.entries_count);
  } else {
    free(entries);
    entries = NULL;
  }
}

// Checks if key is reserved and returns its default length
int get_reserved_string_len(char *key) {
  int len = 0;
  if (!strcmp(key, "CATEGORY") || !strcmp(key, "FORMAT")) {
    len = 4;
  } else if (!strcmp(key, "APP_VER") || !strcmp(key, "CONTENT_VER") || !strcmp(key, "VERSION")) {
    len = 8;
  } else if (!strcmp(key, "INSTALL_DIR_SAVEDATA") || !strcmp(key, "TITLE_ID")) {
    len = 12;
  } else if (!strcmp(key, "SERVICE_ID_ADDCONT_ADD_1") ||
    !strcmp(key, "SERVICE_ID_ADDCONT_ADD_2") ||
    !strcmp(key, "SERVICE_ID_ADDCONT_ADD_3") ||
    !strcmp(key, "SERVICE_ID_ADDCONT_ADD_4") ||
    !strcmp(key, "SERVICE_ID_ADDCONT_ADD_5") ||
    !strcmp(key, "SERVICE_ID_ADDCONT_ADD_6") ||
    !strcmp(key, "SERVICE_ID_ADDCONT_ADD_7")) {
    len = 20;
  } else if (!strcmp(key, "CONTENT_ID")) {
    len = 48;
  } else if (!strcmp(key, "PROVIDER") || !strcmp(key, "TITLE") ||
    !strcmp(key, "PROVIDER_00") || !strcmp(key, "TITLE_00") ||
    !strcmp(key, "PROVIDER_01") || !strcmp(key, "TITLE_01") ||
    !strcmp(key, "PROVIDER_02") || !strcmp(key, "TITLE_02") ||
    !strcmp(key, "PROVIDER_03") || !strcmp(key, "TITLE_03") ||
    !strcmp(key, "PROVIDER_04") || !strcmp(key, "TITLE_04") ||
    !strcmp(key, "PROVIDER_05") || !strcmp(key, "TITLE_05") ||
    !strcmp(key, "PROVIDER_06") || !strcmp(key, "TITLE_06") ||
    !strcmp(key, "PROVIDER_07") || !strcmp(key, "TITLE_07") ||
    !strcmp(key, "PROVIDER_08") || !strcmp(key, "TITLE_08") ||
    !strcmp(key, "PROVIDER_09") || !strcmp(key, "TITLE_09") ||
    !strcmp(key, "PROVIDER_10") || !strcmp(key, "TITLE_10") ||
    !strcmp(key, "PROVIDER_11") || !strcmp(key, "TITLE_11") ||
    !strcmp(key, "PROVIDER_12") || !strcmp(key, "TITLE_12") ||
    !strcmp(key, "PROVIDER_13") || !strcmp(key, "TITLE_13") ||
    !strcmp(key, "PROVIDER_14") || !strcmp(key, "TITLE_14") ||
    !strcmp(key, "PROVIDER_15") || !strcmp(key, "TITLE_15") ||
    !strcmp(key, "PROVIDER_16") || !strcmp(key, "TITLE_16") ||
    !strcmp(key, "PROVIDER_17") || !strcmp(key, "TITLE_17") ||
    !strcmp(key, "PROVIDER_18") || !strcmp(key, "TITLE_18") ||
    !strcmp(key, "PROVIDER_19") || !strcmp(key, "TITLE_19") ||
    !strcmp(key, "PROVIDER_20") || !strcmp(key, "TITLE_20") ||
    !strcmp(key, "TITLE_21") || !strcmp(key, "TITLE_22") ||
    !strcmp(key, "TITLE_23") || !strcmp(key, "TITLE_24") ||
    !strcmp(key, "TITLE_25") || !strcmp(key, "TITLE_26") ||
    !strcmp(key, "TITLE_27") || !strcmp(key, "TITLE_28") ||
    !strcmp(key, "TITLE_29")) {
    len = 128;
  } else if (!strcmp(key, "PUBTOOLINFO") || !strcmp(key, "PS3_TITLE_ID_LIST_FOR_BOOT") ||
    !strcmp(key, "SAVE_DATA_TRANSFER_TITLE_ID_LIST_1") ||
    !strcmp(key, "SAVE_DATA_TRANSFER_TITLE_ID_LIST_2") ||
    !strcmp(key, "SAVE_DATA_TRANSFER_TITLE_ID_LIST_3") ||
    !strcmp(key, "SAVE_DATA_TRANSFER_TITLE_ID_LIST_4") ||
    !strcmp(key, "SAVE_DATA_TRANSFER_TITLE_ID_LIST_5") ||
    !strcmp(key, "SAVE_DATA_TRANSFER_TITLE_ID_LIST_6") ||
    !strcmp(key, "SAVE_DATA_TRANSFER_TITLE_ID_LIST_7")) {
    len = 512;
  }
  return len;
}

// Adds a new parameter to memory
void add_param(char *type, char *key, char *value, int no_fail) {
  struct index_table_entry new_entry = {0};
  int new_index = 0;

  // Get new entry's .param_len and .param_max_len
  if (!strcmp(type, "str")) {
    new_entry.param_fmt = 516;
    new_entry.param_max_len = get_reserved_string_len(key);
    new_entry.param_len = strlen(value) + 1;
    if (new_entry.param_max_len < new_entry.param_len) {
      new_entry.param_max_len = new_entry.param_len;
      // 4-byte alignment
      while (new_entry.param_max_len % 4) {
        new_entry.param_max_len++;
      }
    }
  } else { // "int"
    new_entry.param_fmt = 1028;
    new_entry.param_len = 4;
    new_entry.param_max_len = 4;
  }

  // Get new entry's index and offsets
  for (int i = 0; i < header.entries_count; i++) {
    int result = strcmp(key, &key_table.content[entries[i].key_offset]);
    if (result == 0) { // Parameter already exists
      if (no_fail) {
        return;
      } else {
        fprintf(stderr, "Could not add \"%s\": parameter already exists.\n", key);
        exit(1);
      }
    } else if (result < 0) {
      new_index = i;
      new_entry.key_offset = entries[i].key_offset;
      new_entry.data_offset = entries[i].data_offset;
      break;
    } else if (i == header.entries_count - 1) {
      new_index = i + 1;
      new_entry.key_offset = entries[i].key_offset +
        strlen(&key_table.content[entries[i].key_offset]) + 1;
      new_entry.data_offset = entries[i].data_offset +
        entries[i].param_max_len;
      break;
    }
  }

  // Make room for the new index table entry by moving the old ones
  header.entries_count++;
  entries = _realloc(entries,
    sizeof(struct index_table_entry) * header.entries_count);
  for (int i = header.entries_count - 1; i > new_index; i--) {
    entries[i] = entries[i - 1];
    entries[i].key_offset += strlen(key) + 1;
    entries[i].data_offset += new_entry.param_max_len;
  }

  // Insert new index table entry
  memcpy(&entries[new_index], &new_entry, sizeof(struct index_table_entry));

  // Resize key table
  key_table.size += strlen(key) + 1;
  key_table.content = _realloc(key_table.content, key_table.size);
  // Move higher indexed keys to make room for new key
  for (int i = key_table.size - 1; i > new_entry.key_offset + strlen(key); i--) {
    key_table.content[i] = key_table.content[i - strlen(key) - 1];
  }
  // Insert new key
  memcpy(&key_table.content[new_entry.key_offset], key, strlen(key) + 1);
  pad_table(&key_table);

  // Resize data table
  expand_data_table(new_entry.data_offset, new_entry.param_max_len);

  // Insert new data
  if (!strcmp(type, "str")) {
    memset(&data_table.content[entries[new_index].data_offset], 0,
      new_entry.param_len); // Overwrite whole space with zeros first
    memcpy(&data_table.content[entries[new_index].data_offset],
      value, strlen(value) + 1); // Then copy new value
  } else if (!strcmp(type, "int")) {
    uint32_t new_value = strtoul(value, NULL, 0);
    memcpy(&data_table.content[entries[new_index].data_offset],
      &new_value, 4);
  }
}

// Overwrites an existing parameter or creates a new one
void set_param(char *type, char *key, char *value) {
  delete_param(key, 1);
  add_param(type, key, value, 1);
}

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

// Prints usage information and exits with exit_code
void print_usage(int exit_code) {
  FILE *output;
  if (exit_code) {
    output = stderr;
  } else {
    output = stdout;
  }
  fprintf(output,
  "Usage: %s [OPTIONS] FILE\n\n"
  "Reads a file to print or modify its SFO parameters.\n"
  "Supported file types:\n"
  "  - PS4 param.sfo (print and modify)\n"
  "  - PS4 disc param.sfo (print only)\n"
  "  - PS4 PKG (print only)\n\n"
  "The modification options (-a/--add, -d/--delete, -e/--edit, -s/--set) can be\n"
  "used multiple times. Modifications are done in memory first, in the order in\n"
  "which they appear in the program's command line arguments.\n"
  "If any modification fails, all changes are discarded and no data is written:\n\n"
  "  Modification  Fail condition\n"
  "  --------------------------------------\n"
  "  Add           Parameter already exists\n"
  "  Delete        Parameter not found\n"
  "  Edit          Parameter not found\n"
  "  Set           None\n\n"
  "Options:\n"
  "  -a, --add TYPE PARAMETER VALUE  Add a new parameter, not overwriting existing\n"
  "                                  data. TYPE must be either \"int\" or \"str\".\n"
  "  -d, --delete PARAMETER          Delete specified parameter.\n"
  "      --debug                     Print debug information.\n"
  "      --decimal                   Display integer values as decimal numerals.\n"
  "  -e, --edit PARAMETER VALUE      Change specified parameter's value.\n"
  "  -f, --force                     Do not abort when modifications fail. Make\n"
  "                                  option --new-file overwrite existing files.\n"
  "  -h, --help                      Print usage information and quit.\n"
  "      --new-file                  If FILE (see above) does not exist, create a\n"
  "                                  new param.sfo file of the same name.\n"
  "  -o, --output-file OUTPUT_FILE   Save the final data to a new file of type\n"
  "                                  \"param.sfo\", overwriting existing files.\n"
  "  -q, --query PARAMETER           Print a parameter's value and quit.\n"
  "                                  If the parameter exists, the exit code is 0.\n"
  "  -s, --set TYPE PARAMETER VALUE  Set a parameter, whether it exists or not,\n"
  "                                  overwriting existing data.\n"
  "  -v, --verbose                   Increase verbosity.\n"
  "      --version                   Print version information and quit.\n"
  ,basename(program_name));
  exit(exit_code);
}

void print_version(void) {
  printf("SFO v%s\n", program_version);
  printf("https://github.com/hippie68/sfo\n");
}

// Finds the param.sfo's offset inside a PS4 PKG file
long int get_ps4_pkg_offset() {
  uint32_t pkg_table_offset;
  uint32_t pkg_file_count;
  fseek(file, 0x00C, SEEK_SET);
  fread(&pkg_file_count, 4, 1, file);
  fseek(file, 0x018, SEEK_SET);
  fread(&pkg_table_offset, 4, 1, file);
  pkg_file_count = bswap_32(pkg_file_count);
  pkg_table_offset = bswap_32(pkg_table_offset);
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
  fread(pkg_table_entry, sizeof (struct pkg_table_entry), pkg_file_count, file);
  for (int i = 0; i < pkg_file_count; i++) {
    if (pkg_table_entry[i].id == 1048576) { // param.sfo ID
      return bswap_32(pkg_table_entry[i].offset);
    }
  }
  fprintf(stderr, "Could not find a param.sfo file inside the PS4 PKG.\n");
  fclose(file);
  exit(1);
}

// Removes the leftmost argument from argv; decrements argc
int shift(int *pargc, char **pargv[]) {
  // Exit and print usage information if there is nothing left to shift
  if (*pargc <= 0) {
    fprintf(stderr, "A required argument is missing.\n");
    print_usage(1);
    exit(1);
  }

  (*pargv)++;
  (*pargc)--;
  return 0;
}

// Converts a string to uppercase
void toupper_string(char *string) {
  while (*string) {
    string[0] = toupper(string[0]);
    string++;
  }
}

// Frees all previously malloc'ed pointers; used for memory leak tests
void clean_exit(void) {
  if (commands) free(commands);
  if (entries) free(entries);
  if (key_table.content) free(key_table.content);
  if (data_table.content) free(data_table.content);
  if (file) fclose(file);
}

// Creates an empty param.sfo file
void create_param_sfo(char *file_name) {
  header.magic = 1179865088;
  header.version = 257;
  header.key_table_offset = 20;
  header.data_table_offset = 20;
  header.entries_count = 0;
  save_to_file(file_name);
}

int main(int argc, char *argv[]) {
  atexit(clean_exit);

  char *input_file_name = NULL;
  char *output_file_name = NULL;

  // Parse command line arguments
  program_name = argv[0];
  shift(&argc, &argv);
  while (argc) {
    // Parse file names
    if (argv[0][0] != '-') {
      if (input_file_name == NULL) {
        input_file_name = argv[0];
      } else {
        fprintf(stderr, "Only 1 input file is allowed. Conflicting file names:\n"
          "  \"%s\"\n  \"%s\"\n", input_file_name, argv[0]);
        print_usage(1);
      }
    // Parse options
    } else if (!strcmp(argv[0], "-a") || !strcmp(argv[0], "--add")) {
      commands = _realloc(commands, sizeof(struct command) * (commands_count + 1));
      commands[commands_count].cmd = cmd_add;
      shift(&argc, &argv);
      // TYPE
      if (strcmp(argv[0], "str") && strcmp(argv[0], "int")) {
        fprintf(stderr, "Option --add: TYPE must be \"int\" or \"str\".\n");
        print_usage(1);
      }
      commands[commands_count].param.type = argv[0];
      shift(&argc, &argv);
      // NAME
      toupper_string(argv[0]);
      commands[commands_count].param.key = argv[0];
      shift(&argc, &argv);
      // VALUE
      commands[commands_count].param.value = argv[0];
      commands_count++;
    } else if (!strcmp(argv[0], "--new-file")) {
      option_new_file = 1;
    } else if (!strcmp(argv[0], "-d") || !strcmp(argv[0], "--delete")) {
      commands = _realloc(commands, sizeof(struct command) * (commands_count + 1));
      commands[commands_count].cmd = cmd_delete;
      shift(&argc, &argv);
      // NAME
      toupper_string(argv[0]);
      commands[commands_count].param.key = argv[0];
      commands_count++;
    } else if (!strcmp(argv[0], "--debug")) {
      option_debug = 1;
    } else if (!strcmp(argv[0], "--decimal")) {
      option_decimal = 1;
    } else if (!strcmp(argv[0], "-e") || !strcmp(argv[0], "--edit")) {
      commands = _realloc(commands, sizeof(struct command) * (commands_count + 1));
      commands[commands_count].cmd = cmd_edit;
      shift(&argc, &argv);
      // NAME
      toupper_string(argv[0]);
      commands[commands_count].param.key = argv[0];
      shift(&argc, &argv);
      // VALUE
      commands[commands_count].param.value = argv[0];
      commands_count++;
    } else if (!strcmp(argv[0], "-f") || !strcmp(argv[0], "--force")) {
        option_force = 1;
    } else if (!strcmp(argv[0], "-h") || !strcmp(argv[0], "--help")) {
      print_usage(0);
    } else if (!strcmp(argv[0], "-o") || !strcmp(argv[0], "--output-file")) {
      shift(&argc, &argv);
      output_file_name = argv[0];
    } else if (!strcmp(argv[0], "-q") || !strcmp(argv[0], "--query")) {
      shift(&argc, &argv);
      toupper_string(argv[0]);
      if (!query_string) {
        query_string = argv[0];
      } else {
        fprintf(stderr, "Only 1 search is allowed. Conflicting search strings:\n"
          "  \"%s\"\n, \"%s\"\n.\n", query_string, argv[0]);
        exit(1);
      }
    } else if (!strcmp(argv[0], "-s") || !strcmp(argv[0], "--set")) {
      commands = _realloc(commands, sizeof(struct command) *
        (commands_count + 1));
      commands[commands_count].cmd = cmd_set;
      shift(&argc, &argv);
      // TYPE
      if (strcmp(argv[0], "str") && strcmp(argv[0], "int")) {
        fprintf(stderr, "Option --set: TYPE must be \"int\" or \"str\".\n");
        print_usage(1);
      }
      commands[commands_count].param.type = argv[0];
      shift(&argc, &argv);
      // NAME
      toupper_string(argv[0]);
      commands[commands_count].param.key = argv[0];
      shift(&argc, &argv);
      // VALUE
      commands[commands_count].param.value = argv[0];
      commands_count++;
    } else if (!strcmp(argv[0], "-v") || !strcmp(argv[0], "--verbose")) {
      option_verbose = 1;
    } else if (!strcmp(argv[0], "--version")) {
      print_version();
      exit(0);
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[0]);
      print_usage(1);
    }
    shift(&argc, &argv);
  }

  // DEBUG: Print parsing results
  if (option_debug) {
    fprintf(stderr, "Command line parsing results:\n\n");
    if (input_file_name == NULL) { // printf + null pointer = undefined behavior
      fprintf(stderr, "input_file_name: NULL\n");
    } else {
      fprintf(stderr, "input_file_name: \"%s\"\n", input_file_name);
    }
    if (output_file_name == NULL) {
      fprintf(stderr, "output_file_name: NULL\n");
    } else {
      fprintf(stderr, "output_file_name: \"%s\"\n", output_file_name);
    }
    fprintf(stderr, "option_debug: %d\n", option_debug);
    fprintf(stderr, "option_decimal: %d\n", option_decimal);
    fprintf(stderr, "option_force: %d\n", option_force);
    fprintf(stderr, "option_new_file: %d\n", option_new_file);
    fprintf(stderr, "option_verbose: %d\n", option_verbose);
    if (query_string == NULL) {
      fprintf(stderr, "query_string: NULL\n");
    } else {
      fprintf(stderr, "query_string: \"%s\"\n", query_string);
    }
    fprintf(stderr, "commands_count: %d\n", commands_count);
    for (int i = 0; i < commands_count; i++) {
      fprintf(stderr, "Command %d:\n", i);
      fprintf(stderr, "  .cmd: %d (", commands[i].cmd);
      char *cmd;
      switch (commands[i].cmd) {
        case 0: cmd = "add"; break;
        case 1: cmd = "delete"; break;
        case 2: cmd = "edit"; break;
        case 3: cmd = "set"; break;
      }
      fprintf(stderr, "%s)\n", cmd);
      fprintf(stderr, "  .param.type: %d\n", commands[i].param.type);
      if (commands[i].param.key) {
        fprintf(stderr, "  .param.key: \"%s\"\n", commands[i].param.key);
      } else {
        fprintf(stderr, "  .param.key: NULL\n", commands[i].param.key);
      }
      if (commands[i].param.value) {
        fprintf(stderr, "  .param.value: \"%s\"\n", commands[i].param.value);
      } else  {
        fprintf(stderr, "  .param.value: NULL\n");
      }
    }
    fprintf(stderr, "\n");
  }

  // Open file
  if (!input_file_name) {
    fprintf(stderr, "Please specify a file name.\n");
    print_usage(1);
  }
  // Optionally create file before opening it
  if (option_new_file) {
    if (!option_force && !access(input_file_name, F_OK)) {
      fprintf(stderr, "File \"%s\" already exists.\n", input_file_name);
      exit(1);
    } else {
      create_param_sfo(input_file_name);
    }
  }
  file = fopen(input_file_name, "rb"); // Read only
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", input_file_name);
    exit(1);
  }

  // Get SFO header offset
  uint32_t magic;
  fread(&magic, 4, 1, file);
  if (magic == 1414415231) { // PS4 PKG file
    fseek(file, get_ps4_pkg_offset(), SEEK_SET);
  } else if (magic == 1128612691) { // Disc param.sfo
    fseek(file, 0x800, SEEK_SET);
  } else if (magic == 1179865088) { // Param.sfo file
    rewind(file);
  } else {
    fprintf(stderr, "Param.sfo magic number not found.\n");
    exit(1);
  }

  // Load file contents
  load_header(file);
  load_entries(file);
  load_key_table(file);
  load_data_table(file);

  if (option_debug) {
    fprintf(stderr, "Memory before running commands:\n\n");
    print_header();
    print_entries();
    print_key_table();
    print_data_table();
  }

  // If there are any queued commands, run them and save the file
  if (commands_count) {
    if (magic == 1414415231) {
      fprintf(stderr, "Cannot edit PKG files.\n");
      exit(1);
    }
    if (magic == 1128612691) {
      fprintf(stderr, "Cannot edit disc param.sfo files.\n");
      exit(1);
    }

    for (int i = 0; i < commands_count; i++) {
      switch (commands[i].cmd) {
        case cmd_add:
          add_param(commands[i].param.type, commands[i].param.key,
            commands[i].param.value, option_force);
          break;
        case cmd_delete:
          delete_param(commands[i].param.key, option_force);
          break;
        case cmd_edit:
          edit_param(commands[i].param.key, commands[i].param.value,
            option_force);
          break;
        case cmd_set:
          set_param(commands[i].param.type, commands[i].param.key,
            commands[i].param.value);
          break;
      }
    }

    if (option_debug) {
      fprintf(stderr, "Memory after running commands:\n\n"
        "Header's table offsets will be updated when saving the file.\n\n");
      print_header();
      print_entries();
      print_key_table();
      print_data_table();
    }

    if (output_file_name) {
      save_to_file(output_file_name);
    } else {
      save_to_file(input_file_name);
    }

    if (query_string) {
      return print_param(query_string);
    }
  } else {
    if (output_file_name) {
      save_to_file(output_file_name);
    }

    if (query_string) {
      return print_param(query_string);
    } else {
      print_params();
    }
  }

  return 0;
}
