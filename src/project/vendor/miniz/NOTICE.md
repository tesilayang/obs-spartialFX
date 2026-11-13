# miniz 3.1.0

Upstream: https://github.com/richgel999/miniz
Pinned tag: 3.1.0
Pinned commit: 174573d60290f447c13a2b1b3405de2b96e27d6c
Downloaded source: https://raw.githubusercontent.com/richgel999/miniz/174573d60290f447c13a2b1b3405de2b96e27d6c/<filename>
License: MIT; original LICENSE included verbatim.

The upstream C and header files are unmodified. miniz_export.h is a local static-library export shim (MINIZ_EXPORT expands to nothing). Build with MINIZ_NO_STDIO and MINIZ_NO_ZLIB_COMPATIBLE_NAMES. Archive file access uses Unicode-aware Qt QFile callbacks. ZIP local/central header and data-descriptor checks are implemented by project_archive.cpp because the upstream header validator guesses small-member descriptor widths from the archive-wide ZIP64 mode. The library's streamed decompression and CRC validation remain active.

SHA256 of files as vendored:
0115478d567121238cf6cc1c0c361926cf07a49d9e4c9e66da97fac6a01646b3  LICENSE
6320fa10a5d21db9f78594d92745bc27593c1e6bed657104c22da0a9bd687b49  miniz_common.h
9de25b57776125d11982a806ee6ea5badd85107bebe745f13970ecd9626f0503  miniz_export.h
30aa8bf68ebcd6d15f03e1db3c7e72d70a038d4a44e74e69193185ea9187ecd0  miniz_tdef.c
d62da02953c3834dfcbde4db026c6d9345013ef9b0e53185131016a3a4e48c33  miniz_tdef.h
ff9cc5426df23a78070b5b8fa2dcb7aea68faac466169a883b4ffb8727532350  miniz_tinfl.c
da4850920fdf09f8877d9affabe1ebba1852ce4b1ebbd121f2ba7a7ea8e57e5d  miniz_tinfl.h
3c1f90797c02ddc9d6bfcdf7079f5ea6f6212f257ed8745f2334dd021d697669  miniz_zip.c
b062a5def545445c73fd1d664e35a89165dfb0d996ab848f507244e819cfcf04  miniz_zip.h
f23204efee2bfaffb72a974d2e28c64c3c1d9787144b5c5890f129c465e467bd  miniz.c
232abb0b807fd4962a8e61822459dd49baecb61d0e557f23497732978640f6a7  miniz.h
