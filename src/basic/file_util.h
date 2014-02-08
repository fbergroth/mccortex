#ifndef FILE_UTIL_H_
#define FILE_UTIL_H_

// needed for mode_t used by futil_mkpath(const char *path, mode_t mode)
// and futil_get_file_size(const char* path)
#include <sys/stat.h>
#include "string_buffer.h"

// futil_mkpath - ensure all directories in path exist
// Returns 1 on success, 0 on failure
// Adapted from Jonathan Leffler http://stackoverflow.com/a/675193/431087
char futil_mkpath(const char *path, mode_t mode);

boolean futil_file_exists(const char *file);

boolean futil_is_file_readable(const char *file);
// Creates file if it can write
boolean futil_is_file_writable(const char *file);
off_t futil_get_file_size(const char* filepath);

// Open a new output file with unused name
boolean futil_generate_filename(const char *base_fmt, StrBuf *str);
void futil_get_strbuf_of_dir_path(const char *path, StrBuf *dir);
char* futil_get_current_dir(char abspath[PATH_MAX+1]);


// This is the same as futil_safe_fread
#define SAFE_READ(fh,ptr,size,field,path,fatal) {                              \
  size_t _read = fread(ptr,1,size,fh);                                         \
  if(_read != size) {                                                          \
    if(!fatal) return -1;                                                      \
    die("Couldn't read '%s': expected %zu; recieved: %zu; [file: %s]\n",       \
        field, (size_t)size, _read, path);                                     \
  }                                                                            \
}

#define safe_fread(fh,ptr,size,field,path) \
        futil_safe_fread(fh,ptr,size,field,path,__FILE__,__func__,__LINE__)

// fh is where to read from
// ptr is where to load data to
// size is how many bytes to read
// field is the item being read, path is the file being read
// file, line are the position in the codebase
// Moved this to header to help compiler inline
static inline void futil_safe_fread(FILE *fh, void *ptr, size_t size,
                                    const char *field, const char *path,
                                    const char *file, const char *func, int line)
{
  size_t read_nbytes = fread(ptr, 1, size, fh);
  if(read_nbytes != size)
  {
    call_die(file, func, line,
             "Couldn't read '%s': expected %zu; recieved: %zu; [file: %s]\n",
             field, size, read_nbytes, path);
  }
}


#endif /* FILE_UTIL_H_ */
