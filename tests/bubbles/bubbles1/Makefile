SHELL=/bin/bash -euo pipefail

#
# Test we do not call certain bubbles, including:
# - haploid bubbles (haploid colour on both branches)
# - serial bubbles - bubbles whose branches share a unitig, e.g.
#         _   _
#       _/ \_/ \_
#        \_/ \_/
#

#                     1 0/0 1/1              1b                                      2 0/0 1/1             3 0/0 1/1             4 0/0 1/1
# r:AAGTACCAACTCCCCGATaCCTGTGATCATACCAAACTCCCCGATtCCTGTGATCATAAGTAGTTATGTCGCAAAGTCTGAGAGGTTGCGTCTTTGTACGGGCTGTCAGGCCGGGCCATCAGTTCCAGTATTCTGTGTTCGTGCTCAATTTCTACCACACT
# i:AAGTACCAACTCCCCGATaCCTGTGATCATACCA-----------------------AAGTAGTTATGTCGCAAAGTCTGAGAGGTTGCGTCTTTGTACGGGCTGTCAGGCCGGGCCATCAGTTCCAGTATTCTGTGTTCGTGCTCAATTTCTACCACACT
# I:AAGTACCAACTCCCCGATaCCTGTGATCATACCA-----------------------AAGTAGTTATGTCGCAAAGTCTGAGAGGTTGCGTCTTTGTACGGGCTGTCAGGCCGGGCCATCAGTTCCAGTATTCTGTGTTCGTGCTCAATTTCTACCACACT
# s:AAGTACCAACTCCCCGATtCCTGTGATCATACCA-----------------------AAGTAGTTATGTCGCAAAGTCTGAGAGGTaGCGTCTTTGTACGGGCTGTCAtGCCGGGCCATCAGTTCCAGTAaTCTGTGTTCGTGCTCAATTTCTACCACACT
# S:AAGTACCAACTCCCCGATtCCTGTGATCATACCA-----------------------AAGTAGTTATGTCGCAAAGTCTGAGAGGTaGCGTCTTTGTACGGGCTGTCAtGCCGGGCCATCAGTTCCAGTAaTCTGTGTTCGTGCTCAATTTCTACCACACT

#                   5 0/1 0/1                            5b
# r:AAACGTCGGAACAGTGTGCCGTGTAGGCTCCGATACCATAGGTCGGAACAGTGcGCCGTGTAGGCTTCGACCGATGGCGGCCTGGCCTTTTACAATACTATTCGCTA
# i:AAACGTCGGAACAGTGTGCCGTGTAGGCTCCGATACCATAG------------------------TTCGACCGATGGCGGCCTGGCCTTTTACAATACTATTCGCTA
# I:AAACGTCGGAACAGTGcGCCGTGTAGGCTCCGATACCATAG------------------------TTCGACCGATGGCGGCCTGGCCTTTTACAATACTATTCGCTA
# s:AAACGTCGGAACAGTGTGCCGTGTAGGCTCCGATACCATAG------------------------TTCGACCGATGGCGGCCTGGCCTTTTACAATACTATTCGCTA
# S:AAACGTCGGAACAGTGcGCCGTGTAGGCTCCGATACCATAG------------------------TTCGACCGATGGCGGCCTGGCCTTTTACAATACTATTCGCTA

# Complex structures
#                 6 00,01  6b 01,01 6c 00,01
# r:CATCATTTATACGGATACTACAGCACCACAATCTGGTATCGTTAAACAGTAACCGATCAAGGCTAATGTTGCACAATTCGGTG
# i:CATCATTTATACGGATACTACAGCACCACAATCTGGTATCGTTAAACAGTAACCGATCAAGGCTAATGTTGCACAATTCGGTG
# I:CATCATTTATACGGATACTACAGaACCACAATCTGGTATCGTTAAACAGTAACCGATCAAGGCTAATGTTGCACAATTCGGTG
# s:CATCATTTATACGGATACTACAGCACCACAATCTGGTATCGTTAAACAGTAACCGATCAAGGCTAATGTTGCACAATTCGGTG
# S:CATCATTTATACGGtTACTACAGaACCACAATgTGGTATCGTTAAACAGTAACCGATCAAGGCTAATGTTGCACAATTCGGTG

# haploid: 1, 5
# serial: [2,3] [2,3,4] [3,4]
# Expect 2,3,4,6a,6b,6c to be found

CTXDIR=../../..
MCCORTEX31=$(CTXDIR)/bin/mccortex31
VCFNALLELES=$(CTXDIR)/libs/biogrok/vcf-count-alleles
CTXFLANKS=$(CTXDIR)/scripts/cortex_print_flanks.sh
VCFSORT=$(CTXDIR)/libs/biogrok/vcf-sort
BWA=$(CTXDIR)/libs/bwa/bwa

K=11

SAMPLES=ref itchy scratchy
FASTAS=$(SAMPLES:=.fa)
GRAPHS=$(SAMPLES:=.k$(K).ctx)
LINKS=$(SAMPLES:=.k$(K).ctp.gz)

all: bubbles.txt bubbles.raw.vcf

ref.fa:
	(printf '>a\nAAGTACCAACTCCCCGATaCCTGTGATCATACCAAACTCCCCGATtCCTGTGATCATAAGTAGTTATGTCGCAAAGTCTGAGAGGTTGCGTCTTTGTACGGGCTGTCAGGCCGGGCCATCAGTTCCAGTATTCTGTGTTCGTGCTCAATTTCTACCACACT\n';\
	 printf '>b\nAAACGTCGGAACAGTGTGCCGTGTAGGCTCCGATACCATAGGTCGGAACAGTGcGCCGTGTAGGCTTCGACCGATGGCGGCCTGGCCTTTTACAATACTATTCGCTA\n';\
	 printf '>c\nCATCATTTATACGGATACTACAGCACCACAATCTGGTATCGTTAAACAGTAACCGATCAAGGCTAATGTTGCACAATTCGGTG\n';) > $@

itchy.fa:
	(printf '>a\nAAGTACCAACTCCCCGATaCCTGTGATCATACCA-----------------------AAGTAGTTATGTCGCAAAGTCTGAGAGGTTGCGTCTTTGTACGGGCTGTCAGGCCGGGCCATCAGTTCCAGTATTCTGTGTTCGTGCTCAATTTCTACCACACT\n'; \
	 printf '>A\nAAGTACCAACTCCCCGATaCCTGTGATCATACCA-----------------------AAGTAGTTATGTCGCAAAGTCTGAGAGGTTGCGTCTTTGTACGGGCTGTCAGGCCGGGCCATCAGTTCCAGTATTCTGTGTTCGTGCTCAATTTCTACCACACT\n'; \
	 printf '>b\nAAACGTCGGAACAGTGTGCCGTGTAGGCTCCGATACCATAG---------------------TTCGACCGATGGCGGCCTGGCCTTTTACAATACTATTCGCTA\n'; \
	 printf '>B\nAAACGTCGGAACAGTGcGCCGTGTAGGCTCCGATACCATAG---------------------TTCGACCGATGGCGGCCTGGCCTTTTACAATACTATTCGCTA\n'; \
	 printf '>c\nCATCATTTATACGGATACTACAGCACCACAATCTGGTATCGTTAAACAGTAACCGATCAAGGCTAATGTTGCACAATTCGGTG\n'; \
	 printf '>C\nCATCATTTATACGGATACTACAGaACCACAATCTGGTATCGTTAAACAGTAACCGATCAAGGCTAATGTTGCACAATTCGGTG\n';) | tr -d '-' > $@

scratchy.fa:
	(printf '>a\nAAGTACCAACTCCCCGATtCCTGTGATCATACCA-----------------------AAGTAGTTATGTCGCAAAGTCTGAGAGGTaGCGTCTTTGTACGGGCTGTCAtGCCGGGCCATCAGTTCCAGTAaTCTGTGTTCGTGCTCAATTTCTACCACACT\n';\
	 printf '>A\nAAGTACCAACTCCCCGATtCCTGTGATCATACCA-----------------------AAGTAGTTATGTCGCAAAGTCTGAGAGGTaGCGTCTTTGTACGGGCTGTCAtGCCGGGCCATCAGTTCCAGTAaTCTGTGTTCGTGCTCAATTTCTACCACACT\n';\
	 printf '>b\nAAACGTCGGAACAGTGTGCCGTGTAGGCTCCGATACCATAG---------------------TTCGACCGATGGCGGCCTGGCCTTTTACAATACTATTCGCTA\n';\
	 printf '>B\nAAACGTCGGAACAGTGcGCCGTGTAGGCTCCGATACCATAG---------------------TTCGACCGATGGCGGCCTGGCCTTTTACAATACTATTCGCTA\n';\
	 printf '>c\nCATCATTTATACGGATACTACAGCACCACAATCTGGTATCGTTAAACAGTAACCGATCAAGGCTAATGTTGCACAATTCGGTG\n';\
	 printf '>C\nCATCATTTATACGGtTACTACAGaACCACAATgTGGTATCGTTAAACAGTAACCGATCAAGGCTAATGTTGCACAATTCGGTG\n';) | tr -d '-' > $@

%.k$(K).ctx: %.fa
	$(MCCORTEX31) build -k $(K) --sample "$*" --seq $< $@ >& $@.log

%.k$(K).ctp.gz: %.k$(K).ctx %.fa
	$(MCCORTEX31) thread -o $@ --seq $*.fa $< >& $@.log

bubbles.txt: $(GRAPHS) $(LINKS)
	$(MCCORTEX31) bubbles -o $@.gz --haploid 0 \
	  -p 0:ref.k$(K).ctp.gz -p 1:itchy.k$(K).ctp.gz -p 2:scratchy.k$(K).ctp.gz \
	  0:ref.k$(K).ctx 1:itchy.k$(K).ctx 2:scratchy.k$(K).ctx >& $@.log
	gzip -fd $@.gz

flanks.fa: bubbles.txt
	$(CTXFLANKS) $< > $@

# Mapping with BWA
ref.fa.bwt: ref.fa
	$(BWA) index ref.fa

%.sam: %.fa ref.fa.bwt ref.fa
	$(BWA) mem ref.fa $< > $@

bubbles.raw.vcf: bubbles.txt flanks.sam
	$(MCCORTEX31) calls2vcf -F flanks.sam -o $@ bubbles.txt ref.fa >& $@.log

clean:
	rm -rf $(FASTAS) $(GRAPHS) $(LINKS)
	rm -rf bubbles.txt *.log *.vcf* flanks.fa flanks.sam ref*

.PHONY: all clean test
