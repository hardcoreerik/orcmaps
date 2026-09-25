#pragma once

/* OrcMaps-private names for the bundled miniz.

   ESP-IDF's ROM linker scripts (components/esp_rom/<chip>/ld/<chip>.rom.ld)
   define miniz symbols -- tinfl_decompress, mz_adler32, tdefl_* and others --
   as absolute addresses. Those definitions win over the bundled objects, so
   on a device OrcMaps' calls went to the ROM inflater, which was built from
   a different miniz with a different tinfl_decompressor layout (10,992 bytes
   on ESP32-P4 against the bundled 8,364). The ROM then wrote past the state
   OrcMaps had allocated and corrupted the heap on the first inflate.

   Renaming every bundled symbol keeps the device on the same inflater the
   host tests run. The list is: every global symbol the bundled miniz.c and
   miniz_tinfl.c define, plus every miniz symbol any ESP-IDF ROM exports, so
   that a future call to a ROM-only name fails to link instead of silently
   reaching the ROM. tools/check_miniz_symbols.py enforces this on an
   ESP32-P4 build.

   Included from miniz_export.h, which every miniz header and source reads
   before declaring anything. See docs/DEPENDENCY_LEDGER.md. */

/* Defined by miniz.c */
#define miniz_def_alloc_func orcmap_miniz_def_alloc_func
#define miniz_def_free_func orcmap_miniz_def_free_func
#define miniz_def_realloc_func orcmap_miniz_def_realloc_func
#define mz_adler32 orcmap_mz_adler32
#define mz_crc32 orcmap_mz_crc32
#define mz_free orcmap_mz_free
#define mz_version orcmap_mz_version

/* Defined by miniz_tinfl.c */
#define tinfl_decompress orcmap_tinfl_decompress
#define tinfl_decompress_mem_to_callback orcmap_tinfl_decompress_mem_to_callback
#define tinfl_decompress_mem_to_heap orcmap_tinfl_decompress_mem_to_heap
#define tinfl_decompress_mem_to_mem orcmap_tinfl_decompress_mem_to_mem
#define tinfl_decompressor_alloc orcmap_tinfl_decompressor_alloc
#define tinfl_decompressor_free orcmap_tinfl_decompressor_free

/* Exported by ESP-IDF ROMs; compiled out here (MINIZ_NO_DEFLATE_APIS). */
#define tdefl_compress orcmap_tdefl_compress
#define tdefl_compress_buffer orcmap_tdefl_compress_buffer
#define tdefl_compress_mem_to_heap orcmap_tdefl_compress_mem_to_heap
#define tdefl_compress_mem_to_mem orcmap_tdefl_compress_mem_to_mem
#define tdefl_compress_mem_to_output orcmap_tdefl_compress_mem_to_output
#define tdefl_get_adler32 orcmap_tdefl_get_adler32
#define tdefl_get_prev_return_status orcmap_tdefl_get_prev_return_status
#define tdefl_init orcmap_tdefl_init
#define tdefl_write_image_to_png_file_in_memory \
  orcmap_tdefl_write_image_to_png_file_in_memory
#define tdefl_write_image_to_png_file_in_memory_ex \
  orcmap_tdefl_write_image_to_png_file_in_memory_ex
