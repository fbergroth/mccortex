#include "global.h"
#include "path_format2.h"
#include "db_graph.h"
#include "db_node.h"
#include "hash_table.h"
#include "path_store.h"
#include "util.h"
#include "file_util.h"

// Format:
// -- Header --
// "PATHS"<uint32_t:version><uint32_t:kmersize><uint32_t:num_of_cols>
// <uint64_t:num_of_paths><uint64_t:num_path_bytes><uint64_t:num_kmers_with_paths>
// -- Colours --
// <uint32_t:sname_len><uint8_t x sname_len:sample_name> x num_of_cols
// -- Data --
// <uint8_t x num_path_bytes:path_data>
// <binarykmer x num_kmers_with_paths><uint64_t:path_index>

void paths2_header_alloc(PathFileHeader *h, size_t num_of_cols)
{
  size_t i, old_cap = h->capacity;

  if(h->capacity == 0) {
    h->sample_names = malloc2(num_of_cols * sizeof(StrBuf));
  }
  else if(num_of_cols > h->capacity) {
    h->sample_names = realloc2(h->sample_names, num_of_cols * sizeof(StrBuf));
  }

  for(i = old_cap; i < num_of_cols; i++) {
    strbuf_alloc(&h->sample_names[i], 256);
    strbuf_set(&h->sample_names[i], "noname");
  }

  h->capacity = MAX2(old_cap, num_of_cols);
}

void paths2_header_dealloc(PathFileHeader *h)
{
  size_t i;
  if(h->capacity > 0) {
    for(i = 0; i < h->capacity; i++) strbuf_dealloc(&h->sample_names[i]);
    free(h->sample_names);
    h->capacity = 0;
  }
}

// Set path header variables based on PathStore
void paths2_header_update(PathFileHeader *header, const PathStore *paths)
{
  header->num_of_paths = paths->num_of_paths;
  header->num_path_bytes = paths->next - paths->store;
  header->num_kmers_with_paths = paths->num_kmers_with_paths;
}

// Returns number of bytes read or -1 on error (if fatal is false)
int paths2_file_read_header(FILE *fh, PathFileHeader *h,
                            boolean fatal, const char *path)
{
  int bytes_read = 0;
  char sig[6] = {0};

  SAFE_READ(fh, sig, 5, "PATHS", path, fatal);
  SAFE_READ(fh, &h->version, sizeof(uint32_t), "version", path, fatal);
  SAFE_READ(fh, &h->kmer_size, sizeof(uint32_t), "kmer_size", path, fatal);
  SAFE_READ(fh, &h->num_of_cols, sizeof(uint32_t), "num_of_cols", path, fatal);
  SAFE_READ(fh, &h->num_of_paths, sizeof(uint64_t), "num_of_paths", path, fatal);
  SAFE_READ(fh, &h->num_path_bytes, sizeof(uint64_t),
            "num_path_bytes", path, fatal);
  SAFE_READ(fh, &h->num_kmers_with_paths, sizeof(uint64_t),
            "num_kmers_with_paths", path, fatal);

  bytes_read += 5 + sizeof(uint32_t)*3 + sizeof(uint64_t)*3;

  // paths2_header_alloc will only alloc or realloc only if it needs to
  paths2_header_alloc(h, h->num_of_cols);

  // Read sample names
  size_t i;
  uint32_t len;
  StrBuf *sbuf;
  for(i = 0; i < h->num_of_cols; i++)
  {
    sbuf = h->sample_names + i;
    SAFE_READ(fh, &len, sizeof(uint32_t), "sample name length", path, fatal);
    strbuf_ensure_capacity(sbuf, len);
    SAFE_READ(fh, sbuf->buff, len, "sample name", path, fatal);
    sbuf->buff[sbuf->len = len] = '\0';
    bytes_read += sizeof(uint32_t) + len;
  }

  // Checks
  if(h->version < 1 || h->version > 1) {
    if(!fatal) return -1;
    die("file version not supported [version: %u; path: %s]", h->version, path);
  }

  if(strncmp(sig, "PATHS", 5) != 0) {
    if(!fatal) return -1;
    die("File is not valid paths file [path: %s]", path);
  }

  if(h->kmer_size % 2 == 0) {
    if(!fatal) return -1;
    die("kmer size is not an odd number [kmer_size: %u; binary: %s]\n",
        h->kmer_size, path);
  }

  if(h->kmer_size < 3) {
    if(!fatal) return -1;
    die("kmer size is less than three [kmer_size: %u; binary: %s]\n",
        h->kmer_size, path);
  }

  if(h->num_of_cols == 0) {
    if(!fatal) return -1;
    die("number of colours is zero [binary: %s]\n", path);
  }

  return bytes_read;
}

static inline void load_packed_linkedlist(hkey_t node, PathIndex tmpindex,
                                          const uint8_t *tmpdata,
                                          FileFilter *fltr, boolean find,
                                          dBGraph *db_graph)
{
  const uint8_t *packed;
  PathIndex index; PathLen pbytes; boolean added;
  PathStore *store = &db_graph->pdata;

  // Get paths this kmer already has
  index = db_node_paths(db_graph, node);

  do
  {
    packed = tmpdata+tmpindex;
    pbytes = packedpath_pbytes(packed, store);
    index = path_store_find_or_add_packed2(store, index, packed, pbytes,
                                           fltr, find, &added);
    if(added) db_node_paths(db_graph, node) = index;
    tmpindex = packedpath_prev(packed);
  }
  while(tmpindex != PATH_NULL);
}



// If tmppaths != NULL, do merge
// if insert is true, insert missing kmers into the graph
void paths2_format_load(PathFileReader *file, dBGraph *db_graph,
                        boolean insert_missing_kmers)
{
  const PathFileHeader *hdr = &file->hdr;
  FileFilter *fltr = &file->fltr;
  FILE *fh = fltr->fh;
  const char *path = fltr->path.buff;
  PathStore *store = &db_graph->pdata;

  // If you want to use a file filter you must use paths2_format_merge
  assert(fltr->nofilter);
  assert(store->next == store->store);
  assert(store->num_of_paths == 0 && store->num_kmers_with_paths == 0);

  path_file_load_check(file, db_graph);

  // Print some output
  char kmers_str[100], paths_str[100], mem_str[100];
  ulong_to_str(hdr->num_kmers_with_paths, kmers_str);
  ulong_to_str(hdr->num_of_paths, paths_str);
  bytes_to_str(hdr->num_path_bytes, 1, mem_str);

  status("Loading paths: %s paths, %s path-bytes, %s kmers\n",
         paths_str, mem_str, kmers_str);

  size_t i;
  BinaryKmer bkmer;
  hkey_t node;
  boolean found;

  // Load paths
  safe_fread(fh, store->store, hdr->num_path_bytes, "store->store", path);
  store->next = store->store + hdr->num_path_bytes;
  store->num_of_paths = hdr->num_of_paths;
  store->num_kmers_with_paths = hdr->num_kmers_with_paths;

  // Load kmer pointers to paths
  PathIndex index;
  memset(bkmer.b, 0, sizeof(BinaryKmer));

  for(i = 0; i < hdr->num_kmers_with_paths; i++)
  {
    safe_fread(fh, &bkmer, sizeof(BinaryKmer), "bkmer", path);

    if(insert_missing_kmers) {
      node = hash_table_find_or_insert(&db_graph->ht, bkmer, &found);
    }
    else if((node = hash_table_find(&db_graph->ht, bkmer)) == HASH_NOT_FOUND) {
      die("Node missing: %zu [path: %s]", (size_t)node, path);
    }

    safe_fread(fh, &index, sizeof(uint64_t), "kmer_index", path);
    if(index > hdr->num_path_bytes) {
      die("Path index out of bounds [%zu > %zu]",
          (size_t)index, (size_t)hdr->num_path_bytes);
    }

    db_node_paths(db_graph, node) = index;
  }

  // Test that this is the end of the file
  uint8_t end;
  if(fread(&end, 1, 1, fh) != 0)
    warn("End of file not reached when loading! [path: %s]", path);

  fclose(fh);
}

size_t paths2_get_min_usedcols(PathFileReader *files, size_t num_files)
{
  size_t i, ncols, used_cols = path_file_usedcols(&files[0]);

  for(i = 1; i < num_files; i++) {
    ncols = path_file_usedcols(&files[i]);
    used_cols = MAX2(used_cols, ncols);
  }
  return used_cols;
}

// Load 1 or more path files; can be called consecutively
// db_graph.pdata must be big enough to hold all this data or we exit
// tmpdata must be bigger than MAX(files[*].hdr.num_path_bytes)
void paths2_format_merge(PathFileReader *files, size_t num_files,
                         boolean insert_missing_kmers,
                         uint8_t *tmpdata, size_t datasize, dBGraph *db_graph)
{
  (void)datasize;
  if(num_files == 0) return;

  PathStore *store = &db_graph->pdata;

  // Check number of bytes for colour bitset (path in which cols)
  // This should have been dealt with in the setup of the PathStore
  size_t required_ncols = paths2_get_min_usedcols(files, num_files);
  size_t required_nbytes = round_bits_to_bytes(required_ncols);
  assert(required_ncols <= store->num_of_cols);
  assert(required_nbytes <= store->col_bitset_bytes);

  // load files one at a time
  FileFilter *fltr;
  PathFileHeader *hdr;
  FILE *fh; const char *path;
  BinaryKmer bkey;
  hkey_t node;
  PathIndex tmpindex;
  boolean found, find = true;
  size_t i, first_file = 0;

  for(i = 0; i < num_files; i++)
    path_file_load_check(&files[i], db_graph);

  // Load first file into main store
  if(store->next == store->store)
  {
    // Currently no paths loaded
    if(files[0].fltr.nofilter) {
      paths2_format_load(&files[0], db_graph, insert_missing_kmers);
      first_file = 1;
    } else {
      find = false;
      first_file = 0;
    }
  }

  for(i = first_file; i < num_files; i++)
  {
    fltr = &files[i].fltr;
    hdr = &files[i].hdr;
    path = fltr->orig.buff;
    fh = fltr->fh;

    assert(tmpdata != NULL);
    assert(hdr->num_path_bytes <= datasize);
    safe_fread(fh, tmpdata, hdr->num_path_bytes, "paths->store", path);

    // Load kmer pointers to paths
    memset(&bkey, 0, sizeof(BinaryKmer));

    for(i = 0; i < hdr->num_kmers_with_paths; i++)
    {
      safe_fread(fh, &bkey, sizeof(BinaryKmer), "bkey", path);

      if(insert_missing_kmers) {
        node = hash_table_find_or_insert(&db_graph->ht, bkey, &found);
      }
      else if((node = hash_table_find(&db_graph->ht, bkey)) == HASH_NOT_FOUND) {
        die("Node missing: %zu [path: %s]", (size_t)node, path);
      }

      safe_fread(fh, &tmpindex, sizeof(uint64_t), "kmer_index", path);
      if(tmpindex > hdr->num_path_bytes) {
        die("Path index out of bounds [%zu > %zu]",
            (size_t)tmpindex, (size_t)hdr->num_path_bytes);
      }

      // Merge into currently loaded paths
      load_packed_linkedlist(node, tmpindex, tmpdata, fltr, find, db_graph);
    }

    // Test that this is the end of the file
    uint8_t end;
    if(fread(&end, 1, 1, fh) != 0)
      warn("End of file not reached when loading! [path: %s]", path);
  
    find = true;
  }
}

//
// Write
//

// returns number of bytes written
size_t paths2_format_write_header_core(const PathFileHeader *header, FILE *fout)
{
  fwrite("PATHS", 1, 5, fout);
  fwrite(&header->version, sizeof(uint32_t), 1, fout);
  fwrite(&header->kmer_size, sizeof(uint32_t), 1, fout);
  fwrite(&header->num_of_cols, sizeof(uint32_t), 1, fout);
  fwrite(&header->num_of_paths, sizeof(uint64_t), 1, fout);
  fwrite(&header->num_path_bytes, sizeof(uint64_t), 1, fout);
  fwrite(&header->num_kmers_with_paths, sizeof(uint64_t), 1, fout);
  return 5 + sizeof(uint32_t)*3 + sizeof(uint64_t)*3;
}

// returns number of bytes written
size_t paths2_format_write_header(const PathFileHeader *header, FILE *fout)
{
  paths2_format_write_header_core(header, fout);

  size_t i, bytes = 0;
  uint32_t len;
  const StrBuf *buf;

  for(i = 0; i < header->num_of_cols; i++)
  {
    buf = header->sample_names + i;
    len = buf->len;
    fwrite(&len, sizeof(uint32_t), 1, fout);
    fwrite(buf->buff, sizeof(uint8_t), len, fout);
    bytes += sizeof(uint32_t) + len;
  }

  return bytes;
}

static inline void write_optimised_paths(hkey_t node, PathIndex *pidx,
                                         dBGraph *db_graph, FILE *fout)
{
  PathStore *paths = &db_graph->pdata;
  PathIndex curridx, nextidx, newidx;
  PathLen len;
  Orientation orient;
  size_t mem;

  if((curridx = db_node_paths(db_graph, node)) != PATH_NULL)
  {
    db_node_paths(db_graph, node) = *pidx;

    do
    {
      nextidx = packedpath_prev(paths->store+curridx);
      packedpack_len_orient(paths->store+curridx, paths, &len, &orient);
      mem = packedpath_mem(paths->col_bitset_bytes,len);
      *pidx += mem;
      newidx = (nextidx == PATH_NULL ? PATH_NULL : *pidx);
      fwrite(&newidx, sizeof(PathIndex), 1, fout);
      fwrite(paths->store+curridx+sizeof(PathIndex), mem-sizeof(PathIndex), 1, fout);
      curridx = nextidx;
    }
    while(curridx != PATH_NULL);
  }
}

static inline void write_kmer_path_indices(hkey_t node, const dBGraph *db_graph,
                                           FILE *fout)
{
  if(db_node_paths(db_graph, node) != PATH_NULL)
  {
    BinaryKmer bkmer = db_node_bkmer(db_graph, node);
    PathIndex index = db_node_paths(db_graph, node);
    fwrite(&bkmer, sizeof(BinaryKmer), 1, fout);
    fwrite(&index, sizeof(PathIndex), 1, fout);
  }
}

// Corrupts paths so they cannot be used elsewhere
void paths2_format_write_optimised_paths(dBGraph *db_graph, FILE *fout)
{
  PathIndex poffset = 0;
  HASH_TRAVERSE(&db_graph->ht, write_optimised_paths, &poffset, db_graph, fout);
  HASH_TRAVERSE(&db_graph->ht, write_kmer_path_indices, db_graph, fout);
}