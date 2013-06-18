#ifndef SEQ_READER_H_
#define SEQ_READER_H_

#include <inttypes.h>

#include "db_node.h"
#include "file_reader.h"

// Returns index of first kmer or r->seq.end if no kmers
size_t seq_contig_start(const read_t *r, long offset, uint32_t kmer_size,
                        int qual_cutoff, int hp_cutoff);

// *search_start is the next position to pass to seq_contig_start
size_t seq_contig_end(const read_t *r, size_t contig_start, uint32_t kmer_size,
                      int qual_cutoff, int hp_cutoff, size_t *search_start);

// returns offset of the first kmer or -1 if no kmers present
// Adds a single HASH_NOT_FOUND for gaps
int get_nodes_from_read(const read_t *r, int qcutoff, int hp_cutoff,
                        const dBGraph *db_graph, dBNodeBuffer *buffer);

void seq_parse_pe(const char *path1, const char *path2,
                  read_t *r1, read_t *r2, 
                  SeqLoadingPrefs *prefs, SeqLoadingStats *stats,
                  void (*read_func)(read_t *r1, read_t *r2,
                                    int qoffset1, int qoffset2,
                                    SeqLoadingPrefs *prefs,
                                    SeqLoadingStats *stats,
                                    void *ptr),
                  void *reader_ptr);

void seq_parse_se(const char *path, read_t *r1, read_t *r2,
                  SeqLoadingPrefs *prefs, SeqLoadingStats *stats,
                  void (*read_func)(read_t *r1, read_t *r2,
                                    int qoffset1, int qoffset2,
                                    SeqLoadingPrefs *prefs,
                                    SeqLoadingStats *stats,
                                    void *ptr),
                  void *reader_ptr);


void seq_load_into_db_graph(read_t *r1, read_t *r2,
                            int qoffset1, int qoffset2,
                            SeqLoadingPrefs *prefs, SeqLoadingStats *stats,
                            void *ptr);

#define READ_TO_BKMERS(r,kmer_size,qcutoff,hpcutoff,stats,func,...)            \
  (stats)->total_bases_read += (r)->seq.end;                                   \
  size_t _num_contigs = 0;                                                     \
  if((r)->seq.end >= (kmer_size)) {                                            \
    size_t _search_start = 0, _start, _end = 0, _i;                            \
    BinaryKmer _bkmer; Nucleotide _nuc;                                        \
                                                                               \
    while((_start = seq_contig_start((r), _search_start, (kmer_size),          \
                                     (qcutoff),(hpcutoff))) < (r)->seq.end)    \
    {                                                                          \
      _end = seq_contig_end((r), _start, (kmer_size), (qcutoff), (hpcutoff),   \
                            &_search_start);                                   \
      _num_contigs++;                                                          \
      (stats)->total_bases_loaded += _end - _start;                            \
      (stats)->kmers_loaded += (_end - _start) + 1 - (kmer_size);              \
                                                                               \
      binary_kmer_from_str((r)->seq.b + _start, (kmer_size), _bkmer);          \
      func(_bkmer, ##__VA_ARGS__);                                             \
                                                                               \
      for(_i = _start+(kmer_size); _i < _end; _i++)                            \
      {                                                                        \
        _nuc = binary_nuc_from_char((r)->seq.b[_i]);                      \
        binary_kmer_left_shift_add(_bkmer, (kmer_size), _nuc);                 \
        func(_bkmer, ##__VA_ARGS__);                                           \
      }                                                                        \
    }                                                                          \
    if(_num_contigs == 0) (stats)->total_bad_reads++;                          \
    else (stats)->total_good_reads++;                                          \
  }

#endif /* SEQ_READER_H_ */
